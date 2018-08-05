#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <alloca.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/epoll.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <linux/limits.h>
#include <linux/if_link.h>

// support Big Endian systems as MIPS
#include <endian.h>

#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

// listen backlog size of TCP socket
#define LISTEN_NUM 16
// max bcast ifaces
#define BC_FD_NUM 5

// Str1ker, 06.08.2018: I think we don't need epoll anymore.
// There are 3 socket groups: TCP, UDP main, UDP broadcast
// So an application of 5 threads is the best choise.
#define THREAD_UDP_MAIN 0
#define THREAD_UDP_BCAST 1
#define THREAD_TCP_MAP 2
#define THREAD_UDP_ANNOUNCE 3

// Data required for broadcast
struct bc_data {
    int broadcast_fd;
    struct sockaddr_in bc_addr;
};

// Thread[0] function - udp server port number and address broadcast
void *send_broadcast(void *argv) {
    struct ifaddrs *ifaddr;
    const int bc_enable = 1;
	VECTOR ifaces;
	lassert(vector_init(&ifaces, BC_FD_NUM));

    __syscall(getifaddrs(&ifaddr));

    while(ifaddr != NULL) {
        if(
		       ifaddr->ifa_addr == NULL
            || ifaddr->ifa_addr->sa_family != AF_INET
            || !(ifaddr->ifa_flags & IFLA_BROADCAST)
	        || ((sockaddr_in*)(ifaddr->ifa_addr))->sin_addr.s_addr == htobe32(INADDR_LOOPBACK)
	    )
            goto next_iface;

        // Initialize socket descriptor
		struct bc_data *iface = malloc(sizeof(struct bc_data));
		int sock;
    	__syscall(sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
		__syscall(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc_enable, sizeof(bc_enable)));

        // Setting up port number and address
		sockaddr_in sin = {
			  .sin_addr.s_addr = ((sockaddr_in*)(ifaddr->ifa_ifu.ifu_broadaddr))->sin_addr.s_addr
			, .sin_port = htobe16(BCSSERVER_BCAST_PORT)
			, .sin_family = AF_INET
		};
		iface->broadcast_fd = sock;
		iface->bc_addr = sin;

		__vector_val_t val = { .ptr = iface };
		lassert(vector_push_back(&ifaces, val));

        ALOGD("Beacons will be sent to %s:%hu\n", inet_ntoa(sin.sin_addr), be16toh(sin.sin_port));

next_iface:
        ifaddr = ifaddr->ifa_next;
    }

	lassert(vector_shrink_to_fit(&ifaces));

    BCSBEACON beacon = {
          .magic = htobe64(BCSBEACON_MAGIC)
        , .port = htobe16(BCSSERVER_DEFAULT_PORT)
		, .proto_ver = htobe16(BCSPROTO_VERSION)
        , .description = "Curses-Strike Server v0.2 by Sl1vo4ka"
    };

	size_t n = ifaces.size;
	__vector_val_t *array = vector_array_ptr(&ifaces);
    while(true) {
        for(size_t i = 0; i < n; i++) {
			struct bc_data *iface = array[i].ptr;
            __syscall(sendto2(iface->broadcast_fd, &beacon, sizeof(BCSBEACON), 0, 
				(sockaddr*)&(iface->bc_addr), sizeof(sockaddr_in)));
        }
		// no spam, only one beacon per second
        sleep(1);
    }

    // Unreachable code
    // It would be neccessary in the case of abnormal server termination
	// ReSharper disable once CppUnreachableCode
	for(size_t i = 0; i < ifaces.size; i++) {
		free(ifaces.array[i].ptr);
	}
	vector_free(&ifaces);
    return NULL;
}

/* Thread[1] function - send announces to clients */
void *send_announces(void *argv) {
	// извлекаем полное состояние
	BCSSERVER_FULL_STATE *state = (BCSSERVER_FULL_STATE*)argv;

	int u_fd = state->sock_u;

	// https://www.viva64.com/ru/w/v505/
	// Do not call the alloca() function inside loops
    BCSMSGREPLY *repl = alloca(
		      sizeof(BCSMSGREPLY) 
		    + sizeof(BCSMSGANNOUNCE) 
		    + sizeof(BCSCLIENT_PUBLIC) * BCSSERVER_MAXCLIENTS
	);
    BCSMSGANNOUNCE *ann = (BCSMSGANNOUNCE*)(repl + 1);
    BCSCLIENT_PUBLIC *array = (BCSCLIENT_PUBLIC*)(ann + 1);

	repl->type = BCSREPLT_ANNOUNCE;

    while(true) {
		// берём снимок состояния и работаем над ним
		// 0х600 байт скопируются значительно быстрее чем мы будем их ворошить
		// за это время другой поток может сделать с состоянием что-нибудь важное
		pthread_mutex_lock(&state->mutex_self);
        uint16_t player_count = state->player_count;
		BCSCLIENT state_snap[BCSSERVER_MAXCLIENTS];
		memcpy(state_snap, state->client, sizeof(state->client));
		++(state->frames);
		pthread_mutex_unlock(&state->mutex_self);

		if(player_count > 0) {
			BCSCLIENT_PUBLIC *arr_ptr = array;
			int n = 0;
			// lookup all slots
			// include only registered (non-empty slots)
			// send to actively interacting
            for(int i = 0; i < CLIENTS_NUM; i++) {
				BCSCLIENT *cl_ptr = &state_snap[i];
				// include only registered (non-empty slots), this slot is empty
				if(cl_ptr->public_info.state == BCSCLST_STANDALONE)
					continue;

				// copy struct
				*arr_ptr = cl_ptr->public_info;

				++arr_ptr;
				++n;
			}
			// должно сойтись
			lassert(n == player_count);

			ann->count = player_count;
			size_t annlen =	  sizeof(BCSMSGREPLY)
							+ sizeof(BCSMSGANNOUNCE) 
							+ sizeof(BCSCLIENT_PUBLIC) * player_count;
			// для прикола проштампуем пакет
			bcsproto_new_packet((BCSMSG*)repl);
			n = 0;
			// lookup all slots
            for(int i = 0; i < CLIENTS_NUM; i++) {
				BCSCLIENT *cl_ptr = &state->client[i];
				// assume that state machine is OK
				// and none of these states possible without good endpoint
				// send to actively interacting
                if(
		               cl_ptr->public_info.state == BCSCLST_CONNECTED
					|| cl_ptr->public_info.state == BCSCLST_PLAYING
					|| cl_ptr->public_info.state == BCSCLST_RESPAWNING
                ) { // if clients[i] is not NULL
                    ann->index_self = n;
					sendto(u_fd, repl, annlen, 0
		                , (sockaddr*)&(cl_ptr->private_info.endpoint), sizeof(sockaddr_in));
					++n;
                }
            }
        }
        usleep(33333);
    }
    // Unreachable code
    // It will be neccessary before servers abnornal termination
	// ReSharper disable once CppUnreachableCode
    return NULL;
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

		// ignore beacons
		if(rcvd >= 8 && be64toh(*(uint64_t*)cl_msg) == BCSBEACON_MAGIC)
			continue;

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

// по-русски: серваком можно (по идее) управлять там где запустили.
// предварительно нужно завернуть stderr в другой файл/канал, чтобы не мельтешил
// пишем в консоль команды, цикл в конце мейна их обрабатывает. норм? норм.

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
	char mapfile_name[PATH_MAX] = "res/propeller.bcsmap";
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
		mapfile_name[strlen(mapfile_name) - 1] = '\0';
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
	printf("[$] Press Ctrl+D to stop Curses-Strike v0.%d server.\n", BCSPROTO_VERSION);

	while(true) {
		if(fgets(buf, PATH_MAX, stdin) == NULL)
			break;
		buf[strlen(buf) - 1] = '\0';

		if (strcmp(buf, "help") == 0) {
			printf("[$] Someday there will be a help. Now it's empty...\n");
		}
		else {
			printf("[$] Unknown command '%.40s'\n", buf);
		}
	}

	// User pressed Ctrl+D - terminate server
	printf("[$] You pressed Ctrl+D, exiting gracefully\n");

	// TODO: graceful shutdown
    return EXIT_SUCCESS;
}
