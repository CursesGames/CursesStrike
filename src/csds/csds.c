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
#include <string.h>
#include <sys/time.h>
// support Big Endian systems as MIPS
#include <endian.h>

#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"

#define LISTEN_NUM 16
#define TIMEOUT 10
#define CLIENTS_NUM 16
#define BC_FD_NUM 5

// Data for broadcast sockets
struct bc_data {
    int broadcast_fd;
    struct sockaddr_in udp_bc_address;
};

// Socket initialization and udp address binding
int udp_bind(struct sockaddr_in *addr_udp, socklen_t addr_size) {
    int u_fd;

    // Initialize socket descriptor
    __syscall(u_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

    // Setting up port number and address
    addr_udp->sin_family = AF_INET;
    addr_udp->sin_port = htobe16(BCSSERVER_DEFAULT_PORT);
    addr_udp->sin_addr.s_addr = INADDR_ANY;

	// Str1ker, 03.08.2018: reuse addr to allow server & client on the same iface
	int reuse_addr = 1;
	__syscall(setsockopt(u_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)));

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
    addr_tcp->sin_port = htobe16(BCSSERVER_DEFAULT_PORT);
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
    while(ifaddr != NULL && n < BC_FD_NUM) {
        if(ifaddr->ifa_addr == NULL
            || ifaddr->ifa_addr->sa_family != AF_INET
            || !(ifaddr->ifa_flags & IFLA_BROADCAST))
            goto next_iface;

        if(((struct sockaddr_in*)(ifaddr->ifa_addr))->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            // Initialize socket descriptor
            __syscall(bcs[n].broadcast_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

            // Setting up port number and address
            bcs[n].udp_bc_address.sin_family = AF_INET;
            bcs[n].udp_bc_address.sin_port = htobe16(BCSSERVER_BCAST_PORT);
            bcs[n].udp_bc_address.sin_addr.s_addr = ((struct sockaddr_in*)(ifaddr->ifa_ifu.ifu_broadaddr))->sin_addr.s_addr;

            // Configure the socket to broadcast
            setsockopt(bcs[n].broadcast_fd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val));
            n++;
        }

next_iface:
        ifaddr = ifaddr->ifa_next;
    }

    BCSBEACON to_send = {
          .magic = htobe64(BCSBEACON_MAGIC)
        , .port = htobe16(BCSSERVER_DEFAULT_PORT)
        , .description = "Curses-Strike Server v0.1 by Sl1vo4ka"
    };

    while(true) {
        for(i = 0; i < n; i++){
            __syscall(sendto(bcs[i].broadcast_fd,
            &(to_send), sizeof(BCSBEACON), 0, 
            (struct sockaddr *) &(bcs[i].udp_bc_address), addr_size));
            ALOGV("beacon sent to %s:%hu\n", inet_ntoa(bcs[i].udp_bc_address.sin_addr), ntohs(bcs[i].udp_bc_address.sin_port));
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
void init_start_xy (BCSMAP *map, uint16_t start_x, uint16_t start_y) {
    int i, j;

    for (i = 0; i < map->height; i++) {
        for (j = 0; j < map->width; j++) {
            if (map->map_primitives[i * map->width + j] == 0) {
                start_y = i; //y-coordinate
                start_x = j; //x-coordinate
                map->map_primitives[i * map->width + j] = 1;
                return;
            }
        }
    }
}

// Put client data into array
int add_client (BCSMAP *map, BCSCLIENT *clients, struct sockaddr_in addr_client) {
    struct timeval tv;
    int i;

    for (i = 0; i < CLIENTS_NUM; i++) {
        if (&clients[i] == NULL) {
            clients[i].private_info.endpoint = addr_client; //client endpoint
            __syscall(gettimeofday(&(clients[i].private_info.time_last_dgram), NULL)); //set current time as the last response time
            init_start_xy(map, clients[i].public_info.position.x, clients[i].public_info.position.y); //init coordinates
            clients[i].public_info.state = BCSCLST_CONNECTING; //init state = wait for map
            clients[i].public_info.direction = BCSDIR_UP; //init direction
            return i;
        }
    }

    return -1;
}

// Search client data in array, returns index of element 
int search_client(BCSCLIENT *clients, struct sockaddr_in addr_client) {
    int i;
    int num;

    // If endpoint is the same as addr_client
    for (i = 0; i < CLIENTS_NUM; i++) {
        if ((clients[i].private_info.endpoint.sin_addr.s_addr == addr_client.sin_addr.s_addr) && 
            (clients[i].private_info.endpoint.sin_port == addr_client.sin_port) && 
            (clients[i].private_info.endpoint.sin_family == addr_client.sin_family)) {
            num = i;
            break;
        }
    }

    return num;
}

// Remove client from array
void delete_client (BCSCLIENT *clients, struct sockaddr_in addr_client) {
    int num;

    num = search_client(clients, addr_client);
    memset(clients + num*sizeof(BCSCLIENT), 0, sizeof(BCSCLIENT));
}

void log_print(BCSCLIENT *clients){
    int i;

    for(i = 0; i < CLIENTS_NUM; i++){
        if(&clients[i] != NULL){
            ALOGD("CLIENT %d\n", i);
            //public info 
            ALOGD("state: %d\n", clients[i].public_info.state); //state - print %d ?
            ALOGD("position: x = %u, y = %u\n", (unsigned int)clients[i].public_info.position.x, (unsigned int)clients[i].public_info.position.y);
            ALOGI("direction: %d\n", clients[i].public_info.direction); //direction %d ?
            //public ext info
            ALOGI("frags: %u\n", (unsigned int)(clients[i].public_ext_info.frags));
            ALOGI("deaths: %u\n", (unsigned int)(clients[i].public_ext_info.deaths));
            ALOGI("deaths: %s\n", clients[i].public_ext_info.nickname);
            //private info
            //must be sin_addr.s_addr but is sin_addr
            ALOGI("endpoint : family = %d, port = %d, address = %s\n", clients[i].private_info.endpoint.sin_family, clients[i].private_info.endpoint.sin_port, inet_ntoa(clients[i].private_info.endpoint.sin_addr));
            ALOGI("last fire time: %ld.%06ld\n", clients[i].private_info.time_last_fire.tv_sec, clients[i].private_info.time_last_fire.tv_usec);
            ALOGI("last dgram time: %ld.%06ld\n", clients[i].private_info.time_last_dgram.tv_sec, clients[i].private_info.time_last_dgram.tv_usec);
        }

    }
}

int main(int argc, char **argv) {
    pthread_attr_t attr; //thread attribute
    pthread_t thread;
    struct sockaddr_in addr_tcp, addr_udp, addr_bc_udp, addr_client;
    struct epoll_event event;
    socklen_t addr_size = sizeof (struct sockaddr_in);
    BCSCLIENT *clients = malloc(sizeof(BCSCLIENT) * CLIENTS_NUM);//clients struct
    BCSMSG cl_msg;
    BCSMSGREPLY serv_msg;
    void *status;
    char msg[BCSDGRAM_MAX];
    int epfd; //event polling instance
    int u_fd, t_fd, s_fd; //udp and tcp socket descriptor
    int result;
    int id;

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
    
    memset(clients, 0, sizeof(BCSCLIENT) * CLIENTS_NUM); //clients array nullification

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
			// Str1ker, 03.08.2018: proto convention
            //__syscall(send (s_fd, &map, 4, 0));
			uint16_t tmp = htobe16(map.width);
			__syscall(send (s_fd, &tmp, 2, 0));
			tmp = htobe16(map.height);
			__syscall(send (s_fd, &tmp, 2, 0));

            __syscall(send (s_fd, map.map_primitives, map.width * map.height, 0));
            printf("map was sent to client\n");

            close(s_fd);
        }


        // Receive message from client by udp, add client data to array
        // and send him his initial coordinates
        if (event.data.fd == u_fd) { //check event
            // Receive message-CONNECT from client 
            __syscall(result = recvfrom(u_fd, &cl_msg, sizeof(BCSMSG), 0, (struct sockaddr*) &addr_client, &addr_size));
			// Str1ker, 03.08.2018: ignore beacon packets
			if(result >= 8 && be64toh(*((uint64_t*)&cl_msg)) == BCSBEACON_MAGIC)
				continue;

            printf("received from client: %.*s\n", result, msg);
            
            // Set initial message parameters
            serv_msg.packet_no = cl_msg.packet_no;
            serv_msg.type = htobe32(BCSREPLT_MAP);

            // Send message-MAP to client
            __syscall(sendto(u_fd, &serv_msg, sizeof(BCSMSGREPLY), 0, (struct sockaddr *) &addr_client, addr_size));

            // Add client to array
            __syscall(id = add_client(&map, clients, addr_client));
            //ALOGD();

            // Send coordinates to client
            __syscall(sendto(u_fd, &(clients[id].public_info.position), sizeof(POINT), 0, (struct sockaddr *) &addr_client, addr_size));
            printf("coordinates were sent to client");
            delete_client(clients, addr_client);

            // Str1ker, 03.08.2018: вернул на место коммит 668e0ba604247ef0e8e34f45b138b620cdd3324f
            // Receive message-CONNECT2
            __syscall(result = recvfrom(u_fd, &cl_msg, sizeof(BCSMSG), 0, (struct sockaddr*) &addr_client, &addr_size));
            printf("received from client: %.*s\n", result, msg);
             // Change client stat if the previous was BCSCLST_CONNECTING
            if(clients[id].public_info.state == BCSCLST_CONNECTING){
                clients[id].public_info.state = BCSCLST_CONNECTED;
            }
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
