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
#include <alloca.h>

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

// Thread[0] function - udp server port number and address broadcast
void *send_broadcast (void *arg) {
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

            ALOGV("beacon sent to %s:%hu\n", inet_ntoa(bcs[n].udp_bc_address.sin_addr), ntohs(bcs[i].udp_bc_address.sin_port));

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
        }
        sleep(1);
    }

    // Unreachable code
    // It will be neccessary before servers abnornal termination
    //pthread_exit (NULL);
}

/* Return current number of clients */
int return_clients_size(BCSCLIENT *clients) {
    int i;
    int num = 0;

    for (i = 0; i < CLIENTS_NUM; i++) {
        if (clients->private_info.endpoint.sin_addr.s_addr != 0) {
            num++;
            clients++;
        }
    }

    return num;
}

/* Thread[1] function - send announces to clients */
void *send_announces(void *arg) {
    BCSCLIENT *clients = ((BCSCLIENT *)arg);
    BCSCLIENT *cl_ptr;
    BCSMSGREPLY *repl;
    BCSMSGANNOUNCE *ann;
    BCSCLIENT_PUBLIC *array;
    struct sockaddr_in *addr_udp;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int player_count;
    int i, j;
    int fd;

    // Initialize socket descriptor
    __syscall(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

    // Setting up port number and address
    addr_udp->sin_family = AF_INET;
    addr_udp->sin_port = htobe16(BCSSERVER_DEFAULT_PORT);
    addr_udp->sin_addr.s_addr = INADDR_ANY;

    // Str1ker, 03.08.2018: reuse addr to allow server & client on the same iface
    int reuse_addr = 1;
    __syscall(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)));

    // Link address with socket descriptor
    __syscall(bind(fd, (struct sockaddr *)addr_udp, addr_size));

    while(1) {
        player_count = return_clients_size(clients);
        repl = alloca(sizeof(BCSMSGREPLY) + sizeof(uint16_t) + sizeof(BCSCLIENT_PUBLIC)*player_count);
        ann = (BCSMSGANNOUNCE *)(repl + 1);
        ann->count = player_count;
        array = (BCSCLIENT_PUBLIC *)(((uint16_t *)ann) + 1); //beginning of BCSCLIENT_PUBLIC
        cl_ptr = clients; //beginning of array clients

        for(i = 0; i < CLIENTS_NUM; i++, cl_ptr++) { //send to all clients
            if(cl_ptr->public_info.state != 0) {// if clients[i] is not NULL
                *array = cl_ptr->public_info; //0 element - client-receiver public_info
                array = (BCSCLIENT_PUBLIC *)(((uint16_t *)ann) + 1); //to the beginning of BCSCLIENT_PUBLIC
                for(j = 0; j < player_count; j++, array++) { //other clients public_info
                    if((j != i) && ((cl_ptr + j)->public_info.state != 0)){ //do not include client-receiver and NULL clients
                        *array = (cl_ptr + j)->public_info;
                    }
                }
            }
            // warning! the pointer to member of packed structure!!!
            __syscall(sendto(fd, repl, sizeof(BCSMSGREPLY) + sizeof(uint16_t) + sizeof(BCSCLIENT_PUBLIC)*player_count, 0, (struct sockaddr *) (void *)(&(cl_ptr->private_info.endpoint)), addr_size));
        }
    }
    // Unreachable code
    // It will be neccessary before servers abnornal termination
    //pthread_exit (NULL);
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
int add_client (BCSMAP *map, BCSCLIENT *clients, struct sockaddr_in addr_client){
    struct timeval tv;
    int i;

    for(i = 0; i < CLIENTS_NUM; i++){
        if(clients[i].private_info.endpoint.sin_addr.s_addr == 0){
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
        if ((clients[i].private_info.endpoint.sin_addr.s_addr == 0) && 
            (clients[i].private_info.endpoint.sin_port == addr_client.sin_port) && 
            (clients[i].private_info.endpoint.sin_family == addr_client.sin_family)) {
            num = i;
            return num;
        }
    }

    return -1;
}


// Remove client from array
void delete_client (BCSCLIENT *clients, struct sockaddr_in addr_client) {
    int num;

    num = search_client(clients, addr_client);
    memset(clients + num*sizeof(BCSCLIENT), 0, sizeof(BCSCLIENT));
}

void log_print_cl_info(BCSCLIENT *clients){
    int i;

    for(i = 0; i < CLIENTS_NUM; i++){
        if(clients[i].private_info.endpoint.sin_addr.s_addr != 0){
            ALOGD("CLIENT %d\n", i);
            //public info 
            ALOGD("state: %d\n", clients[i].public_info.state);
            ALOGD("position: x = %u, y = %u\n", (unsigned int)clients[i].public_info.position.x, (unsigned int)clients[i].public_info.position.y);
            ALOGI("direction: %d\n", clients[i].public_info.direction);
            //public ext info
            ALOGI("frags: %u\n", (unsigned int)(clients[i].public_ext_info.frags));
            ALOGI("deaths: %u\n", (unsigned int)(clients[i].public_ext_info.deaths));
            ALOGI("deaths: %s\n", clients[i].public_ext_info.nickname);
            //private info
            ALOGI("endpoint : family = %d, port = %d, address = %s\n", clients[i].private_info.endpoint.sin_family, clients[i].private_info.endpoint.sin_port, inet_ntoa(clients[i].private_info.endpoint.sin_addr));
            ALOGI("last fire time: %ld.%06ld\n", clients[i].private_info.time_last_fire.tv_sec, clients[i].private_info.time_last_fire.tv_usec);
            ALOGI("last dgram time: %ld.%06ld\n", clients[i].private_info.time_last_dgram.tv_sec, clients[i].private_info.time_last_dgram.tv_usec);
        }

    }
}

int main(int argc, char **argv) {
    pthread_attr_t attr; //thread attribute
    pthread_t thread[2];
    struct sockaddr_in addr_tcp, addr_udp, addr_bc_udp, addr_client;
    struct epoll_event event;
    socklen_t addr_size = sizeof (struct sockaddr_in);
    BCSCLIENT *clients = malloc(sizeof(BCSCLIENT) * CLIENTS_NUM);//clients struct
    BCSMSG cl_msg;
    BCSMSGREPLY serv_msg;
    BCSMAP map;
    void *status;
    int epfd; //event polling instance
    int u_fd, t_fd, s_fd; //udp and tcp socket descriptor
    int result;
    int id;
    
    //map loading
    if(!bcsmap_load("propeller.bcsmap", &map)){
        ALOGE("Could not load map from file\n");
        exit(EXIT_FAILURE);
    }
    
    memset(clients, 0, sizeof(BCSCLIENT) * CLIENTS_NUM); //clients array nullification

    u_fd = udp_bind (&addr_udp, addr_size);
    t_fd = tcp_bind (&addr_tcp, addr_size);

    // Initialize thread attribute
    // Thread attribute `joinable' tells, if the system will wait for its termination
    // Kramarenko said that some systems do not have this attribute by default
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    lassert((result = pthread_create (&thread[0], &attr, send_broadcast, (void *) &addr_udp)) == 0); //create thread for broadcast
    lassert((result = pthread_create (&thread[1], &attr, send_announces, (void *) clients)) == 0); //create thread for announces

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
        if(event.data.fd == u_fd){ //check event
            __syscall(result = recvfrom(u_fd, &cl_msg, sizeof(BCSMSG), 0, (struct sockaddr*) &addr_client, &addr_size));
            // why the client may be denied
            id = search_client(clients, addr_client);
            serv_msg.packet_no = cl_msg.packet_no; // packet to send number

            switch(be32toh(cl_msg.action)){
                case BCSACTION_CONNECT: // client sent CONNECT
                    // Ignore beacon packets
                    if(result >= 8 && be64toh(*((uint64_t*)&cl_msg)) == BCSBEACON_MAGIC)
                        continue;
                    printf("received CONNECT from client\n");
                    // Add client to array
                    switch(add_client(&map, clients, addr_client)){
                        case -1: // clients limit is settled
                            serv_msg.type = htobe32(BCSREPLT_NACK);
                            break;

                        default:
                            clients[id].public_info.state = BCSCLST_CONNECTING; //client state to CONNECTING
                            serv_msg.type = htobe32(BCSREPLT_MAP);
                            log_print_cl_info(clients);
                    }
                    break;
                    
                case BCSACTION_CONNECT2: // client sent CONNECT2
                    printf("received CONNECT2 from client\n");
                    // Change client stat if the previous was BCSCLST_CONNECTING
                    if(clients[id].public_info.state == BCSCLST_CONNECTING){
                        clients[id].public_info.state = BCSCLST_CONNECTED;
                        serv_msg.type = htobe32(BCSREPLT_ACK);
                    }
                    else{
                        //error
                        clients[id].public_info.state = BCSCLST_UNDEF;
                        serv_msg.type = htobe32(BCSREPLT_NACK);
                    }
                    break;
                        
                case BCSACTION_DISCONNECT:
                    delete_client(clients, addr_client);
                    serv_msg.type = htobe32(BCSREPLT_ACK);
                    break;

                case BCSACTION_MOVE:
                    switch(be32toh(cl_msg.un.ints.int_lo)){
                        case BCSDIR_LEFT:
                            if((clients[id].public_info.position.x - 1) == 0){
                                serv_msg.type = htobe32(BCSREPLT_NACK);
                            }
                            else{
                                clients[id].public_info.position.x--;
                                serv_msg.type = htobe32(BCSREPLT_ACK);
                            }
                            break;
                                
                        case BCSDIR_RIGHT:
                            if((clients[id].public_info.position.x + 1) == map.width){
                                serv_msg.type = htobe32(BCSREPLT_NACK);
                            }
                            else{
                                clients[id].public_info.position.x++;
                                serv_msg.type = htobe32(BCSREPLT_ACK);
                            }
                            break;

                        case BCSDIR_UP:
                            if((clients[id].public_info.position.y - 1) == 0){
                                serv_msg.type = htobe32(BCSREPLT_NACK);
                            }
                            else{
                                clients[id].public_info.position.y--;
                                serv_msg.type = htobe32(BCSREPLT_ACK);
                            }
                            break;

                        case BCSDIR_DOWN:
                            if((clients[id].public_info.position.y + 1) == map.height){
                                serv_msg.type = htobe32(BCSREPLT_NACK);
                            }
                            else{
                                clients[id].public_info.position.y++;
                                serv_msg.type = htobe32(BCSREPLT_ACK);
                            }

                            default:
                                serv_msg.type = htobe32(BCSREPLT_NACK);
                        }        
                        break;
                        
                case BCSACTION_FIRE:
                    switch(clients[id].public_info.state){
                        case BCSCLST_CONNECTED:
                            clients[id].public_info.state = BCSCLST_PLAYING;
                            serv_msg.type = htobe32(BCSREPLT_ACK);
                            break;

                        case BCSCLST_PLAYING: //UNDEFINED
                            break;

                        default: //nothing -> error
                            serv_msg.type = htobe32(BCSREPLT_NACK);
                        }
                        break;

                //case BCSACTION_STRAFE: //UNDEFINED
                //    serv_msg.type = htobe32(BCSREPLT_NACK);
                //    break;
                
                case BCSACTION_ROTATE: //UNDEFINED
                    serv_msg.type = htobe32(BCSREPLT_NACK);
                    break;

                case BCSACTION_REQSTATS: //UNDEFINED
                    serv_msg.type = htobe32(BCSREPLT_NACK);
                    break;

                default:
                    serv_msg.type = htobe32(BCSREPLT_NACK);
            }
            __syscall(sendto(u_fd, &serv_msg, sizeof(BCSMSGREPLY), 0, (struct sockaddr *) &addr_client, addr_size));
        }
    }

    // Unreachable code
    // It will be neccessary before servers abnornal termination
    /* for(i = 0; i < 2; i++) {
        pthread_join(thread[i], &status); 
        lassert(status == 0);
    }
    pthread_attr_destroy(&attr);

    free(clients);

    close(t_fd);
    close(u_fd); */

    return(EXIT_SUCCESS);
}

