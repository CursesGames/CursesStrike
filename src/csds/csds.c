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
#include "../libbcsstatemachine/bcsstatemachine.h"
#include <linux/limits.h>

// listen backlog size of TCP socket
#define LISTEN_NUM 16
// max bcast ifaces
#define BC_FD_NUM 5

//Data for broadcast
struct bc_data{
    int broadcast_fd;
    struct sockaddr_in udp_bc_address;
};

// Data for thread[1]
struct udp_data {
    int fd;
    BCSCLIENT *cl_array;
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

            ALOGD("beacons will be sent to %s:%hu\n"
		        , inet_ntoa(bcs[n].udp_bc_address.sin_addr), be16toh(bcs[n].udp_bc_address.sin_port));

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
            __syscall(sendto(bcs[i].broadcast_fd, &to_send, sizeof(BCSBEACON), 0, 
            (struct sockaddr *) &(bcs[i].udp_bc_address), addr_size));
        }
        sleep(1);
    }

    // Unreachable code
    // It would be neccessary in the case of abnormal server termination
    //pthread_exit (NULL);
}

// Return current number of clients
// With the introduction of special `count' variable
// this function might become redundant
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
    BCSCLIENT *clients = ((struct udp_data *)arg)->cl_array;
    BCSCLIENT *cl_ptr;
    BCSMSGREPLY *repl;
    BCSMSGANNOUNCE *ann;
    BCSCLIENT_PUBLIC *array;
    struct sockaddr_in addr_udp;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int player_count;
    int i, j;
    int u_fd = (*((struct udp_data *)arg)).fd;

    while(1) {
        player_count = return_clients_size(clients);
        repl = alloca(sizeof(BCSMSGREPLY) + sizeof(uint16_t) + sizeof(BCSCLIENT_PUBLIC)*player_count);
        (*repl).type = BCSREPLT_ANNOUNCE;
        ann = (BCSMSGANNOUNCE *)(repl + 1);
        ann->count = player_count;
        array = (BCSCLIENT_PUBLIC *)(((uint16_t *)ann) + 1); //beginning of BCSCLIENT_PUBLIC
        cl_ptr = clients; //beginning of array clients

        if(player_count != 0){
            for(i = 0; i < CLIENTS_NUM; i++, cl_ptr++) {
                if((((*cl_ptr).public_info.state == BCSCLST_CONNECTED) //send to active player
                || ((*cl_ptr).public_info.state == BCSCLST_PLAYING)
                || ((*cl_ptr).public_info.state == BCSCLST_RESPAWNING))
                && ((*cl_ptr).private_info.endpoint.sin_addr.s_addr != INADDR_ANY)) {// if clients[i] is not NULL
                    *array = (*cl_ptr).public_info; //0 element - client-receiver public_info
                    array = (BCSCLIENT_PUBLIC *)(((uint16_t *)ann) + 1); //to the beginning of BCSCLIENT_PUBLIC
                    for(j = 0; j < player_count; j++, array++) { //other clients with not error state public_info
                        if((j != i) //do not include client-receiver and NULL clients
                        && ((*(cl_ptr + j)).private_info.endpoint.sin_addr.s_addr != INADDR_ANY)
                        && ((*(cl_ptr + j)).public_info.state != BCSCLST_UNDEF)){ 
                            *array = (*(cl_ptr + j)).public_info;
                        }
                    }
                }
                // warning! the pointer to member of packed structure!!!
                __syscall(sendto(u_fd, repl, sizeof(BCSMSGREPLY) + sizeof(uint16_t) + sizeof(BCSCLIENT_PUBLIC)*player_count, 0, (struct sockaddr *) &((*cl_ptr).private_info.endpoint), addr_size));
            }
        }
        usleep(33333);
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

void *serve_map(void *argv) {
	// извлекаем полное состояние
	BCSSERVER_FULL_STATE *state = (BCSSERVER_FULL_STATE*)argv;
	
    // At this moment, we suppose that struct includes:
    // width (2 bytes), height (2 bytes) and the pointer to primitives

	char map_hdr[4];
	pthread_mutex_lock(&state->mutex_self);
	int t_fd = state->sock_t; // copy descriptor from state
	// жоская адресная арифметика, на деле просто копируем размеры в big-endian
	*(uint16_t*)map_hdr = htobe16(state->map.width);
	*(uint16_t*)(map_hdr + 2) = htobe16(state->map.height);
	uint8_t *map_ptr = state->map.map_primitives;
	size_t map_size = state->map.width * state->map.height;
	pthread_mutex_unlock(&state->mutex_self);

	sockaddr_in addr_client;
	socklen_t sa_len;
	while(true) {
		sa_len = sizeof(addr_client);
		int s_fd;
        __syscall(s_fd = accept(t_fd, (sockaddr*)&addr_client, &sa_len));

        // Str1ker, 03.08.2018: proto convention
		// Str1ker, 06.08.2018: optimization, lol
		__syscall(shutdown(s_fd, SHUT_RD));
		// MSG_WAITALL гарантирует, что все данные будут отправлены без повторных recv
		// (за исключением EINTR наверно, Илья знает)
        __syscall(send(s_fd, &map_hdr, 4, MSG_WAITALL)); 
        __syscall(send(s_fd, map_ptr, map_size, MSG_WAITALL));
        ALOGD("Map sent to client %s:%hu\n"
			, inet_ntoa(addr_client.sin_addr), addr_client.sin_port);

        close(s_fd);
	}

	// ReSharper disable once CppUnreachableCode
	return NULL;
}

void *state_machine(void *argv) {
	// извлекаем полное состояние
	BCSSERVER_FULL_STATE *state = (BCSSERVER_FULL_STATE*)argv;

	pthread_mutex_lock(&state->mutex_self);
	int u_fd = state->sock_u; // copy descriptor from state
	pthread_mutex_unlock(&state->mutex_self);

	// размер буфера должен быть огромным, как и сама дейтаграмма. так, на всякий.
	// мало ли, захотим корову переслать?
	char cl_msg[BCSDGRAM_MAX];

	sockaddr_in addr_client;
	socklen_t sa_len;
	while(true) {
		sa_len = sizeof(addr_client);
		ssize_t rcvd;
        __syscall(rcvd = recvfrom(u_fd, &cl_msg, BCSDGRAM_MAX, 0, (sockaddr*)&addr_client, &sa_len));
        if (bcsstatemachine_process_request(state, &addr_client, (BCSMSG*)cl_msg, rcvd)) {
	        ALOGV("request accepted\n");
        }
        else {
	        ALOGV("request denied\n");
        }
	}

	// ReSharper disable once CppUnreachableCode
	return NULL;
}

// stdin stays in the main thread, for user input
// stdout replies on server admin commands
// stderr is for all extra information and someday will be redirected to a file
// Str1ker, 06.08.2018: I think we don't need epoll anymore.
// There are 3 socket groups: TCP, UDP main, UDP broadcast
// So an application of 5 threads is the best choise.
#define THREAD_UDP_MAIN 0
#define THREAD_UDP_BCAST 1
#define THREAD_TCP_MAP 2
#define THREAD_UDP_ANNOUNCE 3

int main(int argc, char **argv) {
    pthread_attr_t attr; //thread attribute
    pthread_t threads[3];

	// до того, как мы не начали создавать потоки
	// за доступ к state можно не опасаться
	// поэтому в main() до создания потоков не юзаем pthread_mutex_lock()
	BCSSERVER_FULL_STATE state = {
		  .map = {
			  .width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .map_overlay = {
			.width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .mutex_self = PTHREAD_MUTEX_INITIALIZER
		, .mutex_sock = PTHREAD_MUTEX_INITIALIZER
		, .sock_u = -1 // erroneous socket
		, .sock_t = -1
		, .player_count = 0
		, .frames = 0
	};

	// clients array nullification
	memset(&state.client, 0, sizeof(state.client));
	lassert(vector_init(&state.sock_b, BC_FD_NUM));
    
    // map loading
	char mapfile_name[PATH_MAX] = "propeller.bcsmap";
	while(true) {
		if(bcsmap_load(mapfile_name, &(state.map)))
			break;
		ALOGW("Could not load map from file '%s'\n", mapfile_name);
		printf("Enter filename of map in .bcsmap format: ");
		fflush(stdout); // for sure
		if(fgets(mapfile_name, PATH_MAX, stdin) == NULL) {
			ALOGE("Fatal error: fgets() failed\n");
			exit(EXIT_FAILURE);
		}
	}

	socklen_t addr_size = sizeof(sockaddr_in);

	// Initialize UDP socket descriptor
    __syscall(state.sock_u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

    // Setting up port number and address
    sockaddr_in addr_udp = {
		  .sin_addr = INADDR_ANY
		, .sin_port = htobe16(BCSSERVER_DEFAULT_PORT)
		, .sin_family = AF_INET
	};

    // Str1ker, 03.08.2018: reuse addr to allow server & client on the same iface
    int reuse_addr = 1;
    __syscall(setsockopt(state.sock_u, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)));

    // Link address with socket descriptor
    __syscall(bind(state.sock_u, (sockaddr*)&addr_udp, addr_size));

    // Initialize TCP socket descriptor
    __syscall(state.sock_t = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));

    // Setting up port number and address
	sockaddr_in addr_tcp = {
		  .sin_addr = INADDR_ANY
		, .sin_port = htobe16(BCSSERVER_DEFAULT_PORT)
		, .sin_family = AF_INET
	};

    // Link address with socket descriptor
    __syscall(bind(state.sock_t, (sockaddr*)&addr_tcp, addr_size));

    // Run socket listenning
    __syscall(listen(state.sock_t, LISTEN_NUM));

    // Initialize thread attribute
    // Thread attribute `joinable' tells, if the system will wait for its termination
    // Kramarenko said that some systems do not have this attribute by default
	// What if he lied?
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

	// create thread for broadcast
    lassert(pthread_create(&threads[THREAD_UDP_BCAST], &attr, send_broadcast, NULL) == 0);
	// create thread for announces
    lassert(pthread_create(&threads[THREAD_UDP_ANNOUNCE], &attr, send_announces, &state) == 0);

	// Str1ker, 06.08.2018
	// create thread for current map serving
	lassert(pthread_create(&threads[THREAD_TCP_MAP], &attr, serve_map, &state) == 0);
	// create thread for our great state machine!
	lassert(pthread_create(&threads[THREAD_UDP_MAIN], &attr, state_machine, &state) == 0);

	// accept commands from stdin
	char buf[PATH_MAX];
	printf("[$] Welcome to the command prompt of dedicated server.\n");
	printf("[$] Put in your commands, one per line, and press Enter.\n");
	printf("[$] Start with 'help' if you are confused.\n");

	while(true) {
		if(fgets(buf, PATH_MAX, stdin) == NULL)
			break;

		if (strcmp(buf, "help") == 0) {
			printf("[$] Someday there will be a help. Now it's empty...\n");
		}
		else {
			printf("[$] Unknown command '%40s'\n", buf);
		}
	}

	// User pressed Ctrl+D - terminate server
	
    return EXIT_SUCCESS;
}
