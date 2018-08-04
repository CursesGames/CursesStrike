// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <ncursesw/ncurses.h>

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <linux/if_link.h>

#include "../libncurses_util/ncurses_util.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"
#include "../libvector/vector.h"

// catching broadcast messages timeout
#define BCAST_SCAN_TIMEOUT_SEC 4
// assume no more than 3 good ifaces by default
#define BCSIFACES_APPROX 3
// assume no more than 10 good servers by default
#define BCSSERVERS_APPROX 10

// WARNING: this is a workaround for dumb C libraries like musl
typedef union sigval sigval_t;

// this is to shorten access to endpoint struct
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

// taken from Bionicle Commander: https://github.com/Str1ker17/EltexLearning/blob/aa2fddc/src/bc/bc.c#L56
typedef enum {
// white-on-black default terminal scheme
	  CPAIR_DEFAULT = 1
// map primitives
	, CPAIR_CELL_BLANK = CPAIR_DEFAULT
	, CPAIR_CELL_WALL
	, CPAIR_CELL_CRATE
	, CPAIR_CELL_WATER
// players
	, CPAIR_PLAYER_SELF
	, CPAIR_PLAYER_FRIENDLY
	, CPAIR_PLAYER_ENEMY
	, CPAIR_PLAYER_NPC
} cpair_list;

// look at bcsproto.h:
//typedef enum __bcsdir {
//	  BCSDIR_LEFT
//	, BCSDIR_RIGHT
//	, BCSDIR_UP
//	, BCSDIR_DOWN
//} BCSDIRECTION;
const char dirchar[] = { '<', '>', '^', 'v' };

typedef union __bcast_un {
	struct {
		in_addr_t bcast;
		in_addr_t mask;
	} v4;
	uint64_t _vval;
} BCAST_UN;

typedef union __bcast_srv_ep {
	struct __endpoint {
		in_addr_t addr;
		uint16_t port;
		uint16_t zero; // should be 0 after init
	} endpoint;
	uint64_t _vval;
} BCAST_SRV_UN;

typedef BCSBEACON BCAST_SRV;

/*typedef struct {
	pthread_mutex_t *mutex_data;
	pthread_mutex_t *mutex_frame;
	pthread_mutex_t *mutex_sock;
	BCSMAP *map;
	WINDOW *mappad;
	BCSCLIENT_PUBLIC *state_public;
	WINDOW *below;
	size_t ticks;
	int32_t delay_val;
} FPSTHREAD;*/

// I don't bother with packing of this structure now
typedef struct {
	BCSCLIENT_PUBLIC self;
	struct {
		uint32_t count;
		BCSCLIENT_PUBLIC *array;
	} others;
	pthread_mutex_t mutex_self; // this struct data exclusive access
	pthread_mutex_t mutex_frame; // ncurses view exclusive access
	pthread_mutex_t mutex_sock; // udp socket exclusive access
	sockaddr_in endpoint; // Server IP:Port
	int sockfd; // UDP socket
	BCSMAP map;
	BCSMAP map_overlay;
	WINDOW *mappad;
	WINDOW *below;
	size_t frames;
} BCSPLAYER_FULL_STATE;

/*typedef struct {
	pthread_mutex_t *mutex_sock;
	BCSCLIENT_PUBLIC *state_public;
	struct __endpoint server_endpoint;
	int sockfd;
} RCVTHREAD;*/

// Создаёт вектор интерфейсов. Вектор должен быть неинициализированным
size_t init_broadcast_receiver(VECTOR *ipv4_faces) {
	char addrstr[INET_ADDRSTRLEN];
	in_addr_t ifaddr;
	BCAST_UN un;
	size_t count = 0;
	struct ifaddrs *ifap_head;
	__syscall(getifaddrs(&ifap_head));
	struct ifaddrs *ifap = ifap_head;
	lassert(vector_init(ipv4_faces, BCSIFACES_APPROX));
	while(ifap != NULL) {
		if(ifap->ifa_addr != NULL 
			&& ifap->ifa_addr->sa_family == AF_INET 
			&& ifap->ifa_flags & IFLA_BROADCAST
		) {
			ALOGD("iface: %s\n", ifap->ifa_name);
			ifaddr = ((struct sockaddr_in*)ifap->ifa_addr)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &ifaddr, addrstr, INET_ADDRSTRLEN));
			ALOGD("addr: %s\t", addrstr);
			un.v4.mask = ((struct sockaddr_in*)ifap->ifa_netmask)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &(un.v4.mask), addrstr, INET_ADDRSTRLEN));
			logprint("mask: %s\t", addrstr);

			un.v4.bcast = ((struct sockaddr_in*)ifap->ifa_ifu.ifu_broadaddr)->sin_addr.s_addr;
			if((ifaddr | (~(un.v4.mask))) != un.v4.bcast) {
				logprint(ANSI_BKGRD_BRIGHT_RED ANSI_COLOR_WHITE);
			}
			lassert(inet_ntop(AF_INET, &un.v4.bcast, addrstr, INET_ADDRSTRLEN));
			logprint("bcast: %s\t", addrstr);
			logprint(ANSI_CLRST);

			// add to vector
			lassert(vector_push_back(ipv4_faces, un._vval));
			count++;

			logprint("\n");
		}

		ifap = ifap->ifa_next;
		if(ifap == ifap_head)
			break;
	}
	freeifaddrs(ifap_head);
	return count;
}

void *receiver_func(void *argv) {
	BCSPLAYER_FULL_STATE *prm = (BCSPLAYER_FULL_STATE*)argv;
	int sock = prm->sockfd;
	struct __endpoint server = {
		  .addr = prm->endpoint.sin_addr.s_addr
		, .port = prm->endpoint.sin_port
		, .zero = 0
	};
	struct sockaddr_in src;
	socklen_t sa_len = sizeof(src);
	char buf[BCSDGRAM_MAX];

	while(true) {
		ssize_t rcvd;
		__syscall(rcvd = recvfrom(sock, buf, BCSDGRAM_MAX, 0, (sockaddr*)&src, &sa_len));
		if(src.sin_addr.s_addr != server.addr || src.sin_port != server.port) {
			ALOGD("Wrong datagram source: %s:%hu != %s:%hu\n"
				, inet_ntoa(src.sin_addr), be16toh(src.sin_port)
				, inet_ntoa(*(struct in_addr*)&(server.addr)), be16toh(server.port)
			);
			continue;
		}

		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		switch(be32toh(repl->type)) {
			case BCSREPLT_ANNOUNCE:
				ALOGI("Received announce\n");
			break;

			case BCSREPLT_EMERGENCY:
				ALOGW("Received emergency message\n");
			break;

			case BCSREPLT_ACK:
				// everything is OK
			break;

			case BCSREPLT_NACK:
				// something's wrong
				ALOGW("Received NACK, action cancelled\n");
			break;

			case BCSREPLT_MAP:
				ALOGI("Received map change event\n");
			break;

			case BCSREPLT_STATS:
			break;

			case BCSREPLT_NONE: 
			default:
				ALOGW("Server sent message of type = %u, do nothing\n", be32toh(repl->type));
			break;
		}

	}

	// ReSharper disable once CppUnreachableCode
	return NULL;
}

void init_map(WINDOW **pad_ptr, BCSMAP *map) {
	if (*pad_ptr != NULL) {
		// resize
		nassert(wresize(*pad_ptr, map->height, map->width));
	}
	else {
		nassert(*pad_ptr = newpad(map->height, map->width));
	}
	// можно здесь же и отрисовать, это же pad
	WINDOW *pad = *pad_ptr;
	uint8_t *ptr = map->map_primitives;
	nassert(werase(pad));

	//FILE *f = fopen("mapdump.txt", "w");

	for(size_t i = 0; i < map->height; i++) {
		for(size_t j = 0; j < map->width; j++) {
			//char c = '\0';
			switch((BCSMAPPRIMITIVE)*ptr) {
				case PUNIT_OPEN_SPACE:
					nassert(wattron(pad, COLOR_PAIR(CPAIR_CELL_BLANK)));
					// winch puts char under cursor, while waddch adjusts cursor
					nassert(mvwinsch(pad, i, j, ' '));
					nassert(wattroff(pad, COLOR_PAIR(CPAIR_CELL_BLANK)));
					//c = ' ';
				break; // empty cell

				case PUNIT_ROCK:
					nassert(wattron(pad, COLOR_PAIR(CPAIR_CELL_WALL)));
					nassert(mvwinsch(pad, i, j, '#'));
					nassert(wattroff(pad, COLOR_PAIR(CPAIR_CELL_WALL)));
					//c = '#';
				break;

				case PUNIT_BOX:
					nassert(wattron(pad, COLOR_PAIR(CPAIR_CELL_CRATE)));
					nassert(mvwinsch(pad, i, j, 'X'));
					nassert(wattroff(pad, COLOR_PAIR(CPAIR_CELL_CRATE)));
					//c = 'X';
				break;

				case PUNIT_WATER:
					nassert(wattron(pad, COLOR_PAIR(CPAIR_CELL_WATER)));
					nassert(mvwinsch(pad, i, j, '~'));
					nassert(wattroff(pad, COLOR_PAIR(CPAIR_CELL_WATER)));
					//c = '~';
				break;
			}
			//fputc(c, f);
			++ptr;
		}
		//fputc('\n', f);
	}
	//fclose(f);
}

void draw_map(WINDOW *pad, BCSMAP *map) {
	
}

void do_action(BCSPLAYER_FULL_STATE *pfs, BCSACTION action, BCSDIRECTION dir) {
	BCSMSG msg = {
		  .action = htobe32(action)
		, .un.long_p = 0
	};

	switch(action) {
		case BCSACTION_DISCONNECT: break;
		case BCSACTION_MOVE: msg.un.ints.int_lo = dir; break;
		case BCSACTION_FIRE: break;
		case BCSACTION_ROTATE: {
			switch(dir) {
				case BCSDIR_LEFT:
				case BCSDIR_RIGHT:
					msg.un.ints.int_lo = dir;
				break;

				default: ALOGW("Wrong direction for rotation: %u\n", dir); return;
			}
		} break;
		case BCSACTION_REQSTATS: break;

		default: ALOGW("Wrong action type %u\n", action); return;
	}

	// TODO: sendto()
	bcsproto_new_packet(&msg);
	sendto2(pfs->sockfd, &msg, sizeof(msg), 0, (sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
}

void draw_window(BCSPLAYER_FULL_STATE *prm) {
	pthread_mutex_lock(&prm->mutex_frame);
    
    int w, h;
	getmaxyx(stdscr, h, w);

	// what to do there?
    //if(stdscr->_clear) {
	//	stdscr->_clear = false;
	//}

	//if(prm->below->_clear) {
	//prm->below->_clear = false;
	nassert(werase(prm->below));
	pthread_mutex_lock(&prm->mutex_self);
	mvwprintw(prm->below, 0, 1, "Frames: %zu", ++prm->frames);
	pthread_mutex_unlock(&prm->mutex_self);
	//}

	nassert(werase(stdscr));
	nassert(wnoutrefresh(stdscr));
	// copy a part of pad
	// первые два параметра - верхний левый угол pad, с которого берём
	// следующие четыре - куда на экран проецируем
    nassert(pnoutrefresh(prm->mappad, 0, 0, 0, 0, 0 + h, 0 + w));

	// draw player
	nassert(wmove(stdscr, prm->self.position.y, prm->self.position.x));
	wattron(stdscr, COLOR_PAIR(CPAIR_PLAYER_SELF));
	winsch(stdscr, dirchar[prm->self.direction]);
	wattroff(stdscr, COLOR_PAIR(CPAIR_PLAYER_SELF));
	nassert(wnoutrefresh(stdscr));

	// эта панелька наложится поверх карты
	nassert(wnoutrefresh(prm->below));
	// всё готово, можно слать клиенту пачку данных
	nassert(doupdate());
	// порисовали и хватит
	pthread_mutex_unlock(&prm->mutex_frame);
}

void timer_detonate(sigval_t argv) {
	return;
	/*
	BCSPLAYER_FULL_STATE *prm = (BCSPLAYER_FULL_STATE*)(argv.sival_ptr);
	pthread_mutex_lock(&prm->mutex_self);
	++(prm->frames);
	stdscr->_clear = true;
	prm->below->_clear = true;
	pthread_mutex_unlock(&prm->mutex_self);

	draw_window(prm);
	*/
	//return NULL;
}

int main(int argc, char **argv) {
	/////////////////////////
	//    ИНИЦИАЛИЗАЦИЯ    //
	/////////////////////////

	// TODO: быстрое подключение через командную строку
	verbose = true;

	// The library uses the locale which the calling program has initialized.
	// If the locale is not initialized, the library assumes that characters
	// are printable as in ISO-8859-1, to work with certain legacy programs.
	// You should initialize the locale and not rely on specific details of
	// the library when the locale has not been setup.
	// Source: man ncurses, line 30
	// Str1ker, 03.08.2018: от себя. без включения локали карта на экране выводилась криво
	setlocale(LC_ALL, "");

	// Random is good. PRNG is.. enough good.
	srand(time(NULL));

	BCSPLAYER_FULL_STATE pfs = {
		  .mutex_self  = PTHREAD_MUTEX_INITIALIZER
		, .mutex_frame = PTHREAD_MUTEX_INITIALIZER
		, .mutex_sock  = PTHREAD_MUTEX_INITIALIZER
		, .self = { 
			  .state = BCSCLST_UNDEF
			, .direction = BCSDIR_UP
			, .position= {
				  .x = 0
				, .y = 0
			  }
		  }
		, .others = {
			  .count = 0
			, .array = NULL
		  }
		, .map = { // DONE: intialize right <- there
			  .width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .map_overlay = {
			  .width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .mappad = NULL
		, .below = NULL
		, .frames = 0
		, .endpoint = {
			  .sin_addr = INADDR_ANY
			, .sin_port = 0
			, .sin_family = AF_INET
		  }
		, .sockfd = -1 // erroneous socket
	};

	char buf[BCSDGRAM_MAX];
	char addrstr[INET_ADDRSTRLEN];
	// узнать все пригодные интерфейсы
	VECTOR ifaces;

	// ReSharper disable once CppJoinDeclarationAndAssignment
	size_t iface_count;
	int epollfd;
	struct epoll_event ev_catch;
	int reuse_port = 1;

start_bcast_scan:
	// ReSharper disable once CppJoinDeclarationAndAssignment
	iface_count = init_broadcast_receiver(&ifaces);

	int *ubcls = (int*)malloc(sizeof(int) * ifaces.size);
	__syscall(epollfd = epoll_create1(0));
	for(uint i = 0; i < iface_count; i++) {
		struct sockaddr_in sin = {
			  .sin_addr.s_addr = ((BCAST_UN*)(&(ifaces.array[i])))->v4.bcast
			, .sin_port = htobe16(BCSSERVER_BCAST_PORT)
			, .sin_family = AF_INET
		};
		__syscall(ubcls[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
		// reuse addr to allow server & client on the same iface
		__syscall(setsockopt(ubcls[i], SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)));
		__syscall(bind(ubcls[i], (sockaddr*)&sin, sizeof(sin)));

		struct epoll_event evt = {
			  .events = EPOLLIN | EPOLLERR
			, .data.fd = ubcls[i]
		};
		__syscall(epoll_ctl(epollfd, EPOLL_CTL_ADD, ubcls[i], &evt));
	}

	VECTOR servers;
	lassert(vector_init(&servers, 10)); // TODO: get rid of magic number
	uint32_t number = 0;
	struct timeval tv_last, tv_now, tv_diff;
	__syscall(gettimeofday(&tv_last, NULL));

	ALOGI("Scanning for servers, please wait...\n");
	while(true) {
		int ret;
		__syscall(ret = epoll_wait(epollfd, &ev_catch, 1, BCAST_SCAN_TIMEOUT_SEC * 1000));
		if(ret == 0) {
			ALOGI("No broadcast into local networks, exiting.\n");
			ALOGI("Maybe connect to server by IP:Port, or create your own?\n");
			// TODO: ask for endpoint
			//break;
			exit(EXIT_FAILURE);
		}
		// ret should be = 1
		lassert(ret == 1);
		struct sockaddr_in srv_sin;
		socklen_t sa_len = sizeof(srv_sin);

		if(ev_catch.events & EPOLLERR) {
			ALOGE("epoll_wait got EPOLLERR\n");
			continue;
		}

		if(ev_catch.events & EPOLLIN) {
			ssize_t len = recvfrom(ev_catch.data.fd, buf, BCSDGRAM_MAX, 0
				, (sockaddr*)&srv_sin, &sa_len);
			BCSBEACON *beacon = (BCSBEACON*)buf;
			if(be64toh(beacon->magic) != BCSBEACON_MAGIC) {
				// log format is reversed for readability
				ALOGW("Received beacon magic %016lx != %016lx incorrect, skipping\n"
					, beacon->magic, htobe64(BCSBEACON_MAGIC));
				continue;
			}

			if(len < (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon is too short, skipping\n");
				continue;
			}

			if(len > (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon is too long\n");
			}

			// print server if new
			BCAST_SRV_UN srv_new = { 
				.endpoint = { 
					  .addr = srv_sin.sin_addr.s_addr
					, .port = beacon->port
					, .zero = 0
				}
			};

			for(size_t i = 0; i < servers.size; i++) {
				if(servers.array[i] == srv_new._vval) {
					// this server is already listed
					goto next_epevent;
				}
			}

			lassert(vector_push_back(&servers, srv_new._vval));
			lassert(inet_ntop(AF_INET, &srv_sin.sin_addr, addrstr, INET_ADDRSTRLEN));
			printf("\t%s:%hu\t%.*s - %u\n"
				, addrstr, be16toh(beacon->port), BCSBEACON_DESCRLEN, beacon->description
				, number + 1
			);
			++number;
			__syscall(gettimeofday(&tv_last, NULL));
			continue;
		}
next_epevent:
		__syscall(gettimeofday(&tv_now, NULL));
		timersub(&tv_now, &tv_last, &tv_diff);
		if(tv_diff.tv_sec >= BCAST_SCAN_TIMEOUT_SEC) {
			ALOGI("Server scan finished\n");
			break;
		}
	}

	// TODO: close b/cast listeners there?
	for(size_t i = 0; i < iface_count; i++) {
		close(ubcls[i]);
	}
	free(ubcls);
	close(epollfd);

	// if we are there, then servers.size > 0?
	lassert(servers.size > 0);
	uint32_t idx;
	while(true) {
		idx = 1;
		printf("Enter 0 to rescan, or the number of server to connect [1]: ");
		fflush(stdout);
		fgets(buf, 256, stdin); // TODO: buffer overflow, check?
		if(buf[0] == '\n')
			break;
		if(sscanf(buf, "%u", &idx) == 1 && idx <= servers.size)
			break;
		printf("Sorry you can't!\n");
	}

	if(idx == 0) {
		vector_free(&servers);
		vector_free(&ifaces);
		goto start_bcast_scan; // FIXME
	}
	
	// connect to selected server
	BCAST_SRV_UN *srv = (BCAST_SRV_UN*)(&(servers.array[idx - 1]));
	pfs.endpoint.sin_addr.s_addr = srv->endpoint.addr;
	pfs.endpoint.sin_port = srv->endpoint.port;

	lassert(inet_ntop(AF_INET, &(srv->endpoint.addr), addrstr, INET_ADDRSTRLEN) != NULL);
	ALOGI("Connecting to %s:%hu\n", addrstr, ntohs(srv->endpoint.port));

	VERBOSE ALOGV("Creating UDP socket... ");
	__syscall(pfs.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
	struct timeval rcvtimeo = {
		  .tv_sec = (BCSRECV_TIMEO / 1000)
		, .tv_usec = (BCSRECV_TIMEO % 1000)
	};
	__syscall(setsockopt(pfs.sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)));
	VERBOSE logprint("OK.\n");

	VERBOSE ALOGV("Sending CONNECT... ");
	// есть смысл сначала создать структуру, а только потом проштамповать
	// зачем? чтобы метка времени создания была ближе к времени отправки
	BCSMSG msg = {
		  .action = htobe32(BCSACTION_CONNECT)
	};
	bcsproto_new_packet(&msg);
	// TODO: use sendto2
	sysassert(sendto(pfs.sockfd, &msg, sizeof(msg), 0
		, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)) == sizeof(msg));
	VERBOSE logprint("OK.\n");

	ssize_t rcvd;
	struct sockaddr_in sin_src;
	socklen_t src_alen = sizeof(sin_src);
	while(true) {
		__syscall(rcvd = recvfrom(pfs.sockfd, buf, BCSDGRAM_MAX, 0, (sockaddr*)&sin_src, &src_alen));
		if(    sin_src.sin_addr.s_addr != pfs.endpoint.sin_addr.s_addr
			|| sin_src.sin_port        != pfs.endpoint.sin_port
		) {
			ALOGD("Source endpoint mismatch, skipping\n");
			continue;
		}

		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		if(repl->packet_no != msg.packet_no) {
			ALOGD("Packet no mismatch (sent %u, got %u), skipping\n"
				, be32toh(msg.packet_no), be32toh(repl->packet_no));
			continue;
		}

		if(be32toh(repl->type) != BCSREPLT_MAP) {
			ALOGD("Reply type mismatch (expecting %u, got %u), skipping\n"
				, BCSREPLT_MAP, be32toh(repl->type));
			continue;
		}

		// if we are there: source matches, packet no and reply type too
		break;
	}

	VERBOSE ALOGV("Server told to download the map.\n");
	VERBOSE ALOGD("Connecting to the TCP socket... ");
	int sock_tcp;
	__syscall(sock_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	__syscall(connect(sock_tcp, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)));
	VERBOSE logprint("OK.\n");

	VERBOSE ALOGV("Receiving map params... ");
	__syscall(recv(sock_tcp, &pfs.map, 4, MSG_WAITALL)); // FIXME: magic const 4 is a sizeof (w+h)
	pfs.map.width = be16toh(pfs.map.width);
	pfs.map.height = be16toh(pfs.map.height);
	VERBOSE logprint("%hux%hu\n", pfs.map.width, pfs.map.height);

	ssize_t size_in_bytes = pfs.map.width * pfs.map.height;
	pfs.map.map_primitives = (uint8_t*)malloc(size_in_bytes);
	VERBOSE ALOGV("Receiving map data... ");
	__syscall(recv(sock_tcp, pfs.map.map_primitives, size_in_bytes, MSG_WAITALL));
	VERBOSE logprint("%lu bytes OK.\n", size_in_bytes);

	close(sock_tcp);

	msg.action = htobe32(BCSACTION_CONNECT2);
	bcsproto_new_packet(&msg);
	VERBOSE ALOGV("Sending CONNECT2... ");
	sysassert(sendto(pfs.sockfd, &msg, sizeof(msg), 0
		, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)) == sizeof(msg));
	while(true) {
		__syscall(rcvd = recvfrom(pfs.sockfd, buf, BCSDGRAM_MAX, 0, (sockaddr*)&sin_src, &src_alen));
		if(    sin_src.sin_addr.s_addr != pfs.endpoint.sin_addr.s_addr
			|| sin_src.sin_port        != pfs.endpoint.sin_port
		) {
			ALOGD("Source endpoint mismatch, skipping\n");
			continue;
		}

		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		if(repl->packet_no != msg.packet_no) {
			ALOGD("Packet no mismatch (sent %u, got %u), skipping\n"
				, be32toh(msg.packet_no), be32toh(repl->packet_no));
			continue;
		}

		if(be32toh(repl->type) != BCSREPLT_ACK) {
			ALOGD("Reply type mismatch (expecting %u, got %u), skipping\n"
				, BCSREPLT_MAP, be32toh(repl->type));
			continue;
		}

		// if we are there: source matches, packet no and reply type too
		break;
	}
	VERBOSE logprint("OK.\n");

	// прелюдия закончена, мы должны быть на сервере.
	// перенаправляем лог в файл
	sysassert(freopen("cs_client.log", "w", stderr) != NULL);

	// NCurses
	nassert(initscr());
	nassert(raw());
	nassert(keypad(stdscr, true));
	nassert(curs_set(false));
	nassert(noecho());
	//nassert(use_default_colors()); // for transparency?

	nassert(start_color());
	nassert(init_pair(CPAIR_CELL_BLANK, COLOR_WHITE, COLOR_BLACK)); // empty cell
	nassert(init_pair(CPAIR_CELL_WALL, COLOR_BLACK, COLOR_WHITE)); // wall
	nassert(init_pair(CPAIR_CELL_CRATE, COLOR_BLACK, COLOR_YELLOW)); // crate
	nassert(init_pair(CPAIR_CELL_WATER, COLOR_WHITE, COLOR_BLUE)); // wall

	nassert(init_pair(CPAIR_PLAYER_SELF, COLOR_WHITE, COLOR_GREEN)); // me
	nassert(init_pair(CPAIR_PLAYER_ENEMY, COLOR_WHITE, COLOR_RED)); // opposite

	// 2D geometry
	int wnd_ymax, wnd_xmax;
	getmaxyx(stdscr, wnd_ymax, wnd_xmax);

	nassert(pfs.below = newwin(1, wnd_xmax / 2, wnd_ymax - 2, 1));
	nassert(wbkgd(pfs.below, COLOR_PAIR(CPAIR_DEFAULT)));
	pfs.below->_clear = true;

	init_map(&pfs.mappad, &pfs.map);

	// запуск таймера
	struct sigevent sgv = {
		  .sigev_notify = SIGEV_THREAD
		, .sigev_value = { .sival_ptr = &pfs }
		, .sigev_signo = SIGALRM
		, .sigev_notify_function = timer_detonate
        , .sigev_notify_attributes = 0
	};
	timer_t timer_id;
	int timer_fd;
	__syscall(timer_fd = timer_create(CLOCK_MONOTONIC, &sgv, &timer_id));
	struct itimerspec t_interval = { // 30 fps
		  .it_interval = { .tv_sec = 0, .tv_nsec = 33333333 }
		, .it_value    = { .tv_sec = 0, .tv_nsec = 33333333 }
	};
	__syscall(timer_settime(timer_id, 0, &t_interval, NULL));

	while(true) {
		/////////////////////////
		//      ОТРИСОВКА      //
		/////////////////////////
		draw_window(&pfs);

		/////////////////////////
		//         ВВОД        //
		/////////////////////////
		int64_t key = raw_wgetch(stdscr);
		//pthread_mutex_lock(&mutex);
		switch(key) {
			/////////////////////////
			//       ОБРАБОТКА     //
			/////////////////////////

			case 0x3: // Ctrl+C
			case KEY_F(10): // F10
				do_action(&pfs, BCSACTION_DISCONNECT, BCSDIR_UNDEF);
				goto loop_leave;

			case 'a':
			case 'A':
			case KEY_LEFT:  do_action(&pfs, BCSACTION_MOVE,   BCSDIR_LEFT);  break;

			case 'd':
			case 'D':
			case KEY_RIGHT: do_action(&pfs, BCSACTION_MOVE,   BCSDIR_RIGHT); break;

			case 'w':
			case 'W':
			case KEY_UP:    do_action(&pfs, BCSACTION_MOVE,   BCSDIR_UP);    break;

			case 's':
			case 'S':
			case KEY_DOWN:  do_action(&pfs, BCSACTION_MOVE,   BCSDIR_DOWN);  break;

			case 'q':
			case 'Q':       do_action(&pfs, BCSACTION_ROTATE, BCSDIR_LEFT);  break;

			case 'e':
			case 'E':       do_action(&pfs, BCSACTION_ROTATE, BCSDIR_RIGHT); break;

			case ' ':       do_action(&pfs, BCSACTION_FIRE,   BCSDIR_UNDEF); break;

			default: break;
		}
		//pthread_mutex_unlock(&mutex);
	}

loop_leave:
	curs_set(true);
	endwin();

	return 0;
}
