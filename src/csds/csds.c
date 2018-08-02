#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"

#define LISTEN_NUM 16
#define PORT_NUM 2018
#define TIMEOUT 10
#define CLIENTS_NUM 16
#define UDP_MAXLEN 65535
#define NICK_SIZE 20
#define BC_FD_NUM 5

// Data for broadcast sockets
struct bc_data {
    int broadcast_fd;
    struct sockaddr_in udp_bc_address;
};

// Client data
struct client_id{
    struct sockaddr_in cl_address;
    long int last_resp_time;
    char nick[NICK_SIZE];

};

// Socket initialization and udp address binding
int udp_bind(struct sockaddr_in *addr_udp, socklen_t addr_size) {
    int u_fd;

    // Initialize socket descriptor
    __syscall(u_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

    // Setting up port number and address
    addr_udp->sin_family = AF_INET;
    addr_udp->sin_port = htons(PORT_NUM);
    addr_udp->sin_addr.s_addr = INADDR_ANY;

    // Link address with socket descriptor
    __syscall(bind(u_fd, (struct sockaddr *)addr_udp, addr_size));

    return u_fd;
}

// Socket initialization and tcp address binding
int tcp_bind(struct sockaddr_in *addr_tcp, socklen_t addr_size) {
    int t_fd;

    // Initialize socket descriptor
    __syscall(t_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));

    // Setting up port number and address
    addr_tcp->sin_family = AF_INET;
    addr_tcp->sin_port = htons(PORT_NUM);
    addr_tcp->sin_addr.s_addr = INADDR_ANY;

    // Link address with socket descriptor
    __syscall(bind(t_fd, (struct sockaddr *)addr_tcp, addr_size));

    // Run socket listenning
    __syscall(listen(t_fd, LISTEN_NUM));

    return t_fd;
}

// Thread function - udp server port number and address broadcast
void *broadcast (void *arg) {
    struct sockaddr_in udp_address = *((struct sockaddr_in *)arg);
    struct ifaddrs *ifaddr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int n = 0, i;
    const int val = 1;
    struct bc_data bcs[BC_FD_NUM];

    __syscall(getifaddrs(&ifaddr));
    while(ifaddr != NULL && i < BC_FD_NUM) {
        if(ifaddr->ifa_addr == NULL
            || ifaddr->ifa_addr->sa_family != AF_INET
            || !(ifaddr->ifa_flags & IFLA_BROADCAST))
            goto next_iface;

        if(((struct sockaddr_in*)(ifaddr->ifa_addr))->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            // Initialize socket descriptor
            __syscall(bcs[i].broadcast_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

            // Setting up port number and address
            bcs[i].udp_bc_address.sin_family = AF_INET;
            bcs[i].udp_bc_address.sin_port = htons(PORT_NUM + 1);
            bcs[i].udp_bc_address.sin_addr.s_addr = ((struct sockaddr_in*)(ifaddr->ifa_addr))->sin_addr.s_addr;

            // Configure the socket to broadcast
            setsockopt(bcs[i].broadcast_fd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val));
            n++;
        }

next_iface:
        ifaddr = ifaddr->ifa_next;
    }

    while(true) {
        for(i = 0; i < n; i++){
            __syscall(sendto(bcs[i].broadcast_fd,
            &(udp_address), addr_size, 0, 
            (struct sockaddr *) &(bcs[i].udp_bc_address), addr_size));
            printf ("data was sent to client\n");
        }
        sleep(1);
    }

    // Unreachable code
    // It will be neccessary before servers abnornal termination
    //pthread_exit (NULL);
}

// Map filling with '0' - in test version
void create_map (BCSMAP *map) {
    int i, j;

    for (i = 0; i < map->height; i++) {
        for (j = 0; j < map->width; j++) {
            map->map_primitives[i * map->width + j] = 0;
        }
    }
}

// Player coordinates assignment
void init_start_xy (BCSMAP *map, uint16_t *start_xy) {
    int i, j;

    for (i = 0; i < map->height; i++) {
        for (j = 0; j < map->width; j++) {
            if (map->map_primitives[i * map->width + j] == 0) {
                start_xy[0] = i; //y-coordinate
                start_xy[1] = j; //x-coordinate
                map->map_primitives[i * map->width + j] = 1;
                return;
            }
        }
    }
}

// Put client data into array
void add_client (struct client_id *clients, struct sockaddr_in addr_client) {
    int i;

    for (i = 0; i < CLIENTS_NUM; i++) {
        if ((clients[i].cl_address.sin_port == 0) && 
            (clients[i].cl_address.sin_family == 0) && 
            (clients[i].cl_address.sin_addr.s_addr == 0)) {
            clients[i].cl_address = addr_client;
            clients[i].last_resp_time = time (NULL); //set current time as the last response time
            return;
        }
    }
}

// Search client data in array, returns index of element 
int search_client (struct client_id *clients, struct sockaddr_in addr_client) {
    int i;
    int num;

    for (i = 0; i < CLIENTS_NUM; i++) {
        if ((clients[i].cl_address.sin_addr.s_addr == addr_client.sin_addr.s_addr) && 
            (clients[i].cl_address.sin_port == addr_client.sin_port) && 
            (clients[i].cl_address.sin_family == addr_client.sin_family)) {
            num = i;
            break;
        }
    }

    return num;
}

// Remove client from array
void delete_client (struct client_id *clients, struct sockaddr_in addr_client) {
    int num;

    num = search_client(clients, addr_client);
    memset(clients + num*sizeof(struct client_id), 0, sizeof(struct client_id));
}

int main(int argc, char **argv) {
    pthread_attr_t attr; //thread attribute
    pthread_t thread;
    struct sockaddr_in addr_tcp, addr_udp, addr_bc_udp, addr_client;
    struct epoll_event event;
    socklen_t addr_size = sizeof (struct sockaddr_in);
    struct client_id *clients = malloc(sizeof(struct client_id) * CLIENTS_NUM);
    void *status;
    char msg[UDP_MAXLEN];
    int epfd; //event polling instance
    int u_fd, t_fd, s_fd; //udp and tcp socket descriptor
    int result;
    uint16_t start_xy[2]; //initial player coordinates

    //create_map (&map);
    BCSMAP map = {
          .width = 80
        , .height = 24
        , .map_primitives = calloc(80 * 24, 1)
    };

    BCSMAP map_state = {
          .width = map.width
        , .height = map.height
        , .map_primitives = calloc(80 * 24, 1)
    };
    
    memset(clients, 0, sizeof(struct client_id) * CLIENTS_NUM); //clients array nullification

    u_fd = udp_bind (&addr_udp, addr_size);
    t_fd = tcp_bind (&addr_tcp, addr_size);

    // Initialize thread attribute
    // Thread attribute `joinable' tells, if the system will wait for its termination
    // Kramarenko said that some systems do not have this attribute by default
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    lassert((result = pthread_create (&thread, &attr, broadcast, (void*) &addr_udp)) == 0); //thread creation

    // Set up the event polling instance
    __syscall(epfd = epoll_create1 (0));

    // Add new notice to epfd for file with t_fd descriptor
    event.data.fd = t_fd;
    event.events = EPOLLIN | EPOLLET;
    __syscall(epoll_ctl(epfd, EPOLL_CTL_ADD, t_fd, &event));

    // Add new notice to epfd for file with u_fd descriptor
    event.data.fd = u_fd;
    event.events = EPOLLIN | EPOLLET;
    __syscall(epoll_ctl(epfd, EPOLL_CTL_ADD, u_fd, &event));

    while(true) {
        // Lock
        __syscall(result = epoll_wait (epfd, &event, 1, -1));

        // Bad event happened
        if ((event.events & EPOLLERR) ||
            (event.events & EPOLLHUP)) {
            fprintf(stderr, "epoll error\n");
        }

        // Connect with client, send map to him and unconnect
        if (event.data.fd == t_fd) { //check event
            __syscall(s_fd = accept (t_fd, (struct sockaddr *) &addr_client, &addr_size));

            // At this moment, we suppose that struct includes:
            // width (2 bytes), height (2 bytes) and the pointer to primitives
            _Static_assert((sizeof(BCSMAP) - sizeof(void*) == 4), "the size of BCSMAP was changed");
            __syscall(send (s_fd, &map, 4, 0));
            __syscall(send (s_fd, map.map_primitives, map.width * map.height, 0));
            printf("map was sent to client\n");

            close(s_fd);
        }


        // Receive message from client by udp, add client data to array
        // and send him his initial coordinates
        if (event.data.fd == u_fd) { //check event
            // Receive message from client 
            __syscall(result = recvfrom(u_fd, msg, UDP_MAXLEN, 0, (struct sockaddr*)&addr_client, &addr_size));
            printf("received from client: %.*s\n", result, msg);

            // Add client to array
            add_client(clients, addr_client);

            // Choose free map coordinates for new client
            init_start_xy(&map_state, start_xy);

            // Send coordinates to client
            __syscall(sendto(u_fd, &start_xy, sizeof(int)*2, 0, (struct sockaddr *) &addr_client, addr_size));
            printf("coordinates were sent to client");
            delete_client(clients, addr_client);
        }
    }

    // Unreachable code
    // It will be neccessary before servers abnornal termination
    /* pthread_join(thread, &status); 
    lassert(status == 0);
    pthread_attr_destroy(&attr);

    free(clients);

    close(t_fd);
    close(u_fd); */

    return(EXIT_SUCCESS);
}