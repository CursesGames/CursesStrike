#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <alloca.h>
#include <pthread.h>
#include <signal.h>

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
#include "../liblinkedlist/linkedlist.h"
#include "../libbcsgameplay/bcsgameplay.h"
#include "../libbcsstatemachine/clientarray.h" // TODO: remove dependency

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
//#define THREAD_UDP_ANNOUNCE 3
#define THREAD_SPEC_COUNT 3

// WARNING: this is a workaround for dumb C libraries like musl
typedef union sigval sigval_t;

// Data required for broadcast
struct bc_data {
    int broadcast_fd;
    struct sockaddr_in bc_addr;
};

// Thread[0] function - udp server port number and address broadcast
void *send_broadcast(void *argv) {
    struct ifaddrs *ifaddr;
    const int bc_enable = 1;
	VECTOR *ifaces = (VECTOR*)argv;

    __syscall(getifaddrs(&ifaddr));
	struct ifaddrs *ifaddr_head = ifaddr;

    while(ifaddr != NULL) {
		if(ifaddr->ifa_addr != NULL && ifaddr->ifa_addr->sa_family == AF_INET) {
			ALOGI("Server is accessible on %s:%hu\n"
				, inet_ntoa(((sockaddr_in*)ifaddr->ifa_addr)->sin_addr), (uint16_t)BCSSERVER_DEFAULT_PORT);
		}
        if(
		       ifaddr->ifa_addr == NULL
            || ifaddr->ifa_addr->sa_family != AF_INET
            || !(ifaddr->ifa_flags & IFLA_BROADCAST)
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
		memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));
		iface->broadcast_fd = sock;
		iface->bc_addr = sin;

		VECTOR_VALTYPE val = { .ptr = iface };
		lassert(vector_push_back(ifaces, val));

        ALOGD("Beacons will be sent to %s:%hu\n", inet_ntoa(sin.sin_addr), be16toh(sin.sin_port));

next_iface:
        ifaddr = ifaddr->ifa_next;
    }

	freeifaddrs(ifaddr_head);

	if (ifaces->size > 0) {
		lassert(vector_shrink_to_fit(ifaces));
	}
	else {
		ALOGW("You have no broadcast interfaces, connect only by IP:Port\n");
		goto stop_broadcast;
	}

    BCSBEACON beacon = {
          .magic = htobe64(BCSBEACON_MAGIC)
        , .port = htobe16(BCSSERVER_DEFAULT_PORT)
		, .proto_ver = htobe16(BCSPROTO_VERSION)
        , .description = "Curses-Strike Server v0.3 by Sl1vo4ka"
    };
	// for valgrind: initialize bytes after descr str
	//memset(beacon.description, 0, BCSBEACON_DESCRLEN + 1);
	//strcpy(beacon.description, "Curses-Strike Server v0.2 by Sl1vo4ka");

	size_t n = ifaces->size;
	VECTOR_VALTYPE *array = vector_array_ptr(ifaces);
    while(true) {
        for(size_t i = 0; i < n; i++) {
			struct bc_data *iface = array[i].ptr;
            __syscall(sendto(iface->broadcast_fd, &beacon, sizeof(BCSBEACON), 0, 
				(sockaddr*)&(iface->bc_addr), sizeof(sockaddr_in)));
        }
		// no spam, only one beacon per second
        sleep(1);
    }

    // Unreachable code
    // It would be neccessary in the case of abnormal server termination
stop_broadcast:
	/*for(size_t i = 0; i < ifaces.size; i++) {
		free(ifaces.array[i].ptr);
	}*/
	//vector_free(&ifaces);
    return NULL;
}

/* Thread[1] function - send announces to clients */
void send_announces(sigval_t argv) {
	// извлекаем полное состояние
	BCSSERVER_FULL_STATE *state = (BCSSERVER_FULL_STATE*)(argv.sival_ptr);

	// берём снимок состояния и работаем над ним
	// 0х600 байт скопируются значительно быстрее чем мы будем их ворошить
	// за это время другой поток может сделать с состоянием что-нибудь важное
	pthread_mutex_lock(&state->mutex_self);
	int u_fd = state->sock_u;
	uint16_t player_count = state->player_count;
	if(state->bullets.count > 0) {
		LINKED_LIST_ENTRY *lle = NULL;
		LIST_VALTYPE *bullet_ptr = linkedlist_next_r(&state->bullets, &lle);
		// overlay the latest state snapshot changes on the map
		// (TODO) accounting for the destruction of boxes
		// now only the position of clients is taken into account 
		bcsgameplay_map_overlay_create(state);
		while(bullet_ptr != NULL) {
			// tornem, commit faster, oh pleeease
			// yeeees, yeees, i'm here
			// bullet_ptr->ptr is a pointer to BULLET structure
			// there will be something like:
			if(!bcsgameplay_bullet_step(state, bullet_ptr->ptr, BCSBULLET_SPEED)) {
				free(bullet_ptr->ptr);
			    bullet_ptr = linkedlist_throw(&state->bullets, &lle);
			}
			else {
			    bullet_ptr = linkedlist_next_r(&state->bullets, &lle);
			}
		}
	}
	// после обработки пуль количество могло измениться
	uint16_t bullet_count = state->bullets.count;
	BCSCLIENT state_snap[BCSSERVER_MAXCLIENTS];
	++(state->frames);
	memcpy(state_snap, state->client, sizeof(state->client));

	// https://www.viva64.com/ru/w/v505/
	// Do not call the alloca() function inside loops
    BCSMSGREPLY *repl = alloca(
		      sizeof(BCSMSGREPLY) 
		    + sizeof(BCSMSGANNOUNCE) 
		    + sizeof(BCSCLIENT_PUBLIC) * player_count
			+ sizeof(BCSBULLET) * bullet_count
	);
    BCSMSGANNOUNCE *ann = (BCSMSGANNOUNCE*)(repl + 1);
    BCSCLIENT_PUBLIC *array = (BCSCLIENT_PUBLIC*)(ann + 1);
	BCSBULLET *array_bullet = (BCSBULLET*)(array + player_count);

	LIST_VALTYPE *lv;
	LINKED_LIST_ENTRY *lle = NULL;
	int n = 0;
	while((lv = linkedlist_next_r(&state->bullets, &lle)) != NULL) {
		array_bullet[n] = *((BCSBULLET*)lv->ptr);
		// convert to BE
		array_bullet[n].creator_id = htobe16(array_bullet[n].creator_id);
		array_bullet[n].direction = htobe32(array_bullet[n].direction);
		array_bullet[n].x = htobe16(array_bullet[n].x);
		array_bullet[n].y = htobe16(array_bullet[n].y);
		++n;
	}
	// респануть игроков
	struct timeval now;
	__syscall(gettimeofday(&now, NULL));
	for(uint16_t i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
		if(state->client[i].public_info.state == BCSCLST_RESPAWNING) {
			//ALOGD("Now: %lu.%lu, spawn at %lu.%lu"
			//	, now.tv_sec, now.tv_usec
			//	, state->client[i].private_info.time_last_fire.tv_sec
			//	, state->client[i].private_info.time_last_fire.tv_usec
			//);
			if(timercmp(&now, &state->client[i].private_info.time_last_fire, >=)) {
				// state is respawning, nothing bad would happen
				lassert(bcsgameplay_respawn(state, i));
				ALOGD("SPAWN OF %u DONE!!!\n", i);
			}
		}
	}
	pthread_mutex_unlock(&state->mutex_self);

	repl->type = htobe32(BCSREPLT_ANNOUNCE);

	if(player_count > 0) {
		BCSCLIENT_PUBLIC *arr_ptr = array;
		n = 0;
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
			// convert to BE
			arr_ptr->direction = htobe32(arr_ptr->direction);
			arr_ptr->position.x = htobe16(arr_ptr->position.x);
			arr_ptr->position.y = htobe16(arr_ptr->position.y);
			arr_ptr->state = htobe32(arr_ptr->state);

			++arr_ptr;
			++n;
		}
		// должно сойтись
		lassert(n == player_count);

		ann->count = htobe16(player_count);
		ann->count_bullets = htobe16(bullet_count);
		size_t annlen =	  sizeof(BCSMSGREPLY)
						+ sizeof(BCSMSGANNOUNCE) 
						+ sizeof(BCSCLIENT_PUBLIC) * player_count
						+ sizeof(BCSBULLET) * bullet_count;
		// для прикола проштампуем пакет
		bcsproto_new_packet((BCSMSG*)repl);
		n = 0;
		// lookup all slots
        for(int i = 0; i < CLIENTS_NUM; i++) {
			BCSCLIENT *cl_ptr = &state_snap[i];
			// assume that state machine is OK
			// and none of these states possible without good endpoint
			// send to actively interacting
            if(
	               cl_ptr->public_info.state == BCSCLST_CONNECTED
				|| cl_ptr->public_info.state == BCSCLST_PLAYING
				|| cl_ptr->public_info.state == BCSCLST_RESPAWNING
            ) { // if clients[i] is not NULL
                ann->index_self = htobe16(n);
				//ALOGI("Index self = %d\n", n);
				__syscall(sendto(u_fd, repl, annlen, 0
	                , (sockaddr*)&(cl_ptr->private_info.endpoint), sizeof(sockaddr_in)));
				++n;

            }
        }
    }
	pthread_cancel(pthread_self());
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
	        //ALOGV("request accepted\n");
        }
        else {
	        //ALOGV("request denied\n");
        }
	}

	// ReSharper disable once CppUnreachableCode
	return NULL;
}

void dump_state(BCSSERVER_FULL_STATE *state, FILE *f) {
	const char dirchar[] = { '<', '^', '>', 'v' };
	pthread_mutex_lock(&state->mutex_self);
	fprintf(f, "Client slots:\n");
	for(int i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
		BCSCLIENT *cl = &state->client[i];
		fprintf(f, "Slot #%d: ", i);
		switch(cl->public_info.state) {
			case BCSCLST_FREESLOT: fprintf(f, "FREESLOT"); break;
			case BCSCLST_CONNECTING: fprintf(f, "CONNECTING"); break;
			case BCSCLST_CONNECTED: fprintf(f, "CONNECTED"); break;
			case BCSCLST_PLAYING: fprintf(f, "PLAYING"); break;
			case BCSCLST_RESPAWNING: fprintf(f, "RESPAWNING"); break;
		}
		fprintf(f, "(%u), (%hu, %hu), %c (%u)\n", cl->public_info.state
			, cl->public_info.position.x, cl->public_info.position.y
			, dirchar[cl->public_info.direction], cl->public_info.direction);

		fprintf(f, "\tNickname = '%s', %hu:%hu\n", cl->public_ext_info.nickname
			, cl->public_ext_info.frags, cl->public_ext_info.deaths);
		fprintf(f, "\t%s:%hu, seq = %u, (timers hidden)\n"
			, inet_ntoa(cl->private_info.endpoint.sin_addr)
			, cl->private_info.endpoint.sin_port, cl->private_info.last_packet_no);
	}
	fprintf(f, "Bullet objects: %lu\n", state->bullets.count);
	LIST_VALTYPE *lv;
	LINKED_LIST_ENTRY *lle = NULL;
	while((lv = linkedlist_next_r(&state->bullets, &lle)) != NULL) {
		BCSBULLET *bullet = (BCSBULLET*)(lv->ptr);
		fprintf(f, "\tCreator: %hu, '%c', (%hu, %hu)\n"
			, bullet->creator_id, dirchar[bullet->direction], bullet->x, bullet->y);
	}
	BCSMAP map = state->map;
	uint8_t *map_xray = alloca(map.width * map.height);
	fprintf(f, "Map X-Ray: (%hux%hu)\n", map.width, map.height);

	// static
	for(uint16_t y = 0; y < map.height; ++y) {
		for(uint16_t x = 0; x < map.width; ++x) {
			char c;
			switch((BCSMAPPRIMITIVE)(map.map_primitives[y * map.width + x])) {
				case PUNIT_ROCK: c = '#'; break;
				case PUNIT_WATER: c = '~'; break;
				case PUNIT_BOX: c = 'X'; break;
				default: c = ' ';
			}
			map_xray[y * map.width + x] = c;
		}
	}
	// dynamic: players
	for(uint16_t i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
		if(state->client[i].public_info.state == BCSCLST_PLAYING) {
			char c;
			uint32_t offset = state->client[i].public_info.position.y * map.width 
				+ state->client[i].public_info.position.x;
			if (i < 10) c = '0' + i;
			else c = 'A' + (i - 10);
			if (map_xray[offset] == ' ') {
				map_xray[offset] = c;
			}
			else {
				ALOGW("OVERLAY(1) DETECTED AT (%hu, %hu)!!!\n"
					, state->client[i].public_info.position.x, state->client[i].public_info.position.y);
			}
		}
	}
	// dynamic: bullets
	lle = NULL;
	while((lv = linkedlist_next_r(&state->bullets, &lle)) != NULL) {
		BCSBULLET bullet = *(BCSBULLET*)(lv->ptr);
		char c = map_xray[bullet.y * map.width + bullet.x];
		char d = ' ';
		switch(bullet.direction) {
			case BCSDIR_LEFT: case BCSDIR_RIGHT: d = '-'; break;
			case BCSDIR_UP: case BCSDIR_DOWN: d = '|'; break;
		}
		if (c == ' ') 
			map_xray[bullet.y * map.width + bullet.x] = d;
		else if((c == '-' && d == '|') || (c == '|' && d == '-'))
			map_xray[bullet.y * map.width + bullet.x] = '+';
		else
			ALOGW("OVERLAY(2) DETECTED AT (%hu, %hu)!!!\n", bullet.x, bullet.y);
	}

	for(uint16_t y = 0; y < map.height; ++y) {
		for(uint16_t x = 0; x < map.width; ++x) {
			fputc(map_xray[y * map.width + x], f);
		}
		fprintf(f, "\n");
	}
	fclose(f);
	pthread_mutex_unlock(&state->mutex_self);
}

// stdin stays in the main thread, for user input
// stdout replies on server admin commands
// stderr is for all extra information and someday will be redirected to a file

// по-русски: серваком можно (по идее) управлять там где запустили.
// предварительно нужно завернуть stderr в другой файл/канал, чтобы не мельтешил
// пишем в консоль команды, цикл в конце мейна их обрабатывает. норм? норм.

int main(int argc, char **argv) {
    pthread_t threads[THREAD_SPEC_COUNT];
	pthread_attr_t attr[THREAD_SPEC_COUNT]; //thread attribute
	pthread_attr_t attr_timer;

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
	linkedlist_init(&state.bullets);
    
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

	// allocate memory for overlay
	state.map_overlay.width = state.map.width;
	state.map_overlay.height = state.map.height;
	state.map_overlay.map_primitives = (uint8_t*)malloc(state.map.width * state.map.height);

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

	__syscall(setsockopt(state.sock_t, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)));

    // Link address with socket descriptor
    __syscall(bind(state.sock_t, (sockaddr*)&addr_tcp, addr_size));

    // Run socket listenning
    __syscall(listen(state.sock_t, LISTEN_NUM));

    // Initialize thread attribute
    // Thread attribute `joinable' tells, if the system will wait for its termination
    // Kramarenko said that some systems do not have this attribute by default
	// What if he lied?

	// Str1ker, 06.08.2018
	// create thread for current map serving
    pthread_attr_init(&attr[THREAD_TCP_MAP]);
	pthread_attr_setdetachstate(&attr[THREAD_TCP_MAP], PTHREAD_CREATE_JOINABLE);
	lassert(pthread_create(&threads[THREAD_TCP_MAP], &attr[THREAD_TCP_MAP], serve_map, &state) == 0);

	// create thread for our great state machine!
	pthread_attr_init(&attr[THREAD_UDP_MAIN]);
	pthread_attr_setdetachstate(&attr[THREAD_UDP_MAIN], PTHREAD_CREATE_JOINABLE);
	lassert(pthread_create(&threads[THREAD_UDP_MAIN], &attr[THREAD_UDP_MAIN], state_machine, &state) == 0);

	// create thread for broadcast
    pthread_attr_init(&attr[THREAD_UDP_BCAST]);
	pthread_attr_setdetachstate(&attr[THREAD_UDP_BCAST], PTHREAD_CREATE_JOINABLE);
    lassert(pthread_create(&threads[THREAD_UDP_BCAST], &attr[THREAD_UDP_BCAST], send_broadcast, &state.sock_b) == 0);

	// create thread for announces
    pthread_attr_init(&attr_timer);
	pthread_attr_setdetachstate(&attr_timer, PTHREAD_CREATE_DETACHED);
	// Str1ker, 06.08.2018: replace thread to system monotonic clock timer
    //lassert(pthread_create(&threads[THREAD_UDP_ANNOUNCE], &attr, send_announces, &state) == 0);
	struct sigevent sgv = {
		  .sigev_notify = SIGEV_THREAD
		, .sigev_value = { .sival_ptr = &state }
		, .sigev_signo = SIGALRM
		, .sigev_notify_function = send_announces
        , .sigev_notify_attributes = &attr_timer
	};
	timer_t timer_id;
	int timer_fd;
	__syscall(timer_fd = timer_create(CLOCK_MONOTONIC, &sgv, &timer_id));
	struct itimerspec t_interval = { // 30 fps
		  .it_interval = { .tv_sec = 0, .tv_nsec = 33333333 }
		, .it_value    = { .tv_sec = 0, .tv_nsec = 33333333 }
	};
	__syscall(timer_settime(timer_id, 0, &t_interval, NULL));

	if (argc > 1 && strcmp(argv[1], "daemon") == 0) {
		ALOGI("Daemonized mode\n");
		close(STDIN_FILENO);
		pthread_join(threads[THREAD_UDP_MAIN], NULL);
	}
	else {
		usleep(100000);

		// accept commands from stdin
		char buf[PATH_MAX];
		printf("[$] Welcome to the command prompt of dedicated server.\n");
		printf("[$] Put in your commands, one per line, and press Enter.\n");
		printf("[$] Start with 'help' if you are confused.\n");
		printf("[$] Press Ctrl+D to stop Curses-Strike v0.%d server.\n", BCSPROTO_VERSION);

		while(true) {
			uint16_t id;
			if(fgets(buf, PATH_MAX, stdin) == NULL)
				break;
			buf[strlen(buf) - 1] = '\0';

			if (strcmp(buf, "help") == 0) {
				printf("[$] Someday there will be a help. Now it's empty...\n");
			}
			else if (strcmp(buf, "info") == 0) {
				log_print_cl_info(&state);
			}
			else if (strcmp(buf, "dump") == 0) {
				FILE *f = fopen("dump_server.log", "w");
				dump_state(&state, f);
				fclose(f);
				printf("[$] Dump is in 'dump_server.log' for you.\n");
			}
			else if (strcmp(buf, "dump here") == 0) {
				dump_state(&state, stdout);
				printf("[$] Dump is in front of you.\n");
			}
			else if(sscanf(buf, "kick %hu", &id) == 1) {
				if (id <= BCSSERVER_MAXCLIENTS) {
					pthread_mutex_lock(&state.mutex_self);
					if (state.client[id].public_info.state == BCSCLST_FREESLOT) {
						printf("[$] The slot %u is already free.\n", id);
					}
					else {
						BCSMSGREPLY repl = { .type = htobe32(BCSREPLT_SHUTDOWN) };
						sendto2(state.sock_u, &repl, sizeof(BCSMSGREPLY), MSG_DONTWAIT
							,(sockaddr*)&(state.client[id].private_info.endpoint), sizeof(sockaddr_in));
						memset(&state.client[id], 0, sizeof(BCSCLIENT));
						printf("[$] Kicked %u for you.\n", id);
					}
					pthread_mutex_unlock(&state.mutex_self);
				}
				else {
					printf("[$] Sorry you can't!\n");
				}
			}
			else {
				printf("[$] Unknown command '%.40s'\n", buf);
			}
		}

		// User pressed Ctrl+D - terminate server
		printf("[$] You pressed Ctrl+D, exiting gracefully\n");
	}
	// TODO: graceful shutdown
	__syscall(timer_delete(timer_id));
	for(int i = 0; i < THREAD_SPEC_COUNT; ++i) {
		//lassert(pthread_kill(threads[i], SIGTERM) == 0);
		lassert(pthread_cancel(threads[i]) == 0);
		lassert(pthread_join(threads[i], NULL) == 0);
		lassert(pthread_attr_destroy(&attr[i]) == 0);
	}
	// now we are the only thread, don't use mutexes
	for(size_t i = 0; i < state.sock_b.size; ++i) {
		close(((struct bc_data*)(state.sock_b.array[i].ptr))->broadcast_fd);
		free(state.sock_b.array[i].ptr);
	}
	BCSMSGREPLY repl = {
		.type = htobe32(BCSREPLT_SHUTDOWN)
	};
	// уведомить всех игроков
	for(uint16_t i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
		if(state.client[i].public_info.state != BCSCLST_FREESLOT) {
			sendto2(state.sock_u , &repl, sizeof(BCSMSGREPLY), MSG_DONTWAIT
				, (sockaddr*)&(state.client[i].private_info.endpoint), sizeof(sockaddr_in));
		}
	}
	close(state.sock_u);
	close(state.sock_t);
	for(size_t i = 0; i < state.sock_b.size; ++i) {
		free(state.sock_b.array[i].ptr);
	}
	vector_free(&state.sock_b);
	free(state.map.map_primitives);
	free(state.map_overlay.map_primitives);
	linkedlist_clear(&state.bullets);

    return EXIT_SUCCESS;
}
