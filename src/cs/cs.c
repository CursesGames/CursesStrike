// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <ncursesw/ncurses.h>
#include <pthread.h>
#include <signal.h>

#include "../libncurses_util/ncurses_util.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include "../libvector/vector.h"
#include <sys/epoll.h>
#include "../libbcsproto/bcsproto.h"
#include <unistd.h>
#include <sys/time.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 2018
#endif

#ifndef DGRAM_MAX
#define DGRAM_MAX 65535
#endif

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

typedef struct {
	pthread_mutex_t *mutex_data;
	pthread_mutex_t *mutex_frame;
	BCSMAP *map;
	WINDOW *below;
	size_t ticks;
	int32_t delay_val;
	int16_t me_x;
	int16_t me_y;
} FPSTHREAD;

typedef union __bcast_un {
	struct {
		in_addr_t bcast;
		in_addr_t mask;
	} v4;
	uint64_t _vval;
} BCAST_UN;

typedef union __bcast_srv_ep {
	struct {
		in_addr_t addr;
		uint16_t port;
		uint16_t zero; // should be 0 after init
	} endpoint;
	uint64_t _vval;
} BCAST_SRV_UN;

typedef BCSBEACON BCAST_SRV;

void draw_window(FPSTHREAD *prm);

// WARNING: this is a workaround for dump C libraries like musl
typedef union sigval sigval_t;

// Создаёт вектор интерфейсов. Вектор может быть неинициализированным
size_t init_broadcast_receiver(VECTOR *ipv4_faces) {
	char addrstr[INET_ADDRSTRLEN];
	in_addr_t ifaddr;
	BCAST_UN un;
	size_t count = 0;
	struct ifaddrs *ifap_head;
	__syscall(getifaddrs(&ifap_head));
	struct ifaddrs *ifap = ifap_head;
	lassert(vector_init(ipv4_faces, 3)); // assume no more than 3 good ifaces by default
	while(ifap != NULL) {
		if(ifap->ifa_addr != NULL && ifap->ifa_addr->sa_family == AF_INET) {
			ALOGD("iface: %s\n", ifap->ifa_name);
			ifaddr = ((struct sockaddr_in*)ifap->ifa_addr)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &ifaddr, addrstr, INET_ADDRSTRLEN));
			ALOGD("addr: %s\t", addrstr);
			un.v4.mask = ((struct sockaddr_in*)ifap->ifa_netmask)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &(un.v4.mask), addrstr, INET_ADDRSTRLEN));
			logprint("mask: %s\t", addrstr);
			
			if(ifap->ifa_flags & IFLA_BROADCAST) {
				logprint("\t");

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
			}

			logprint("\n");
		}

		ifap = ifap->ifa_next;
		if(ifap == ifap_head)
			break;
	}
	return count;
}

void timer_detonate(sigval_t argv) {
	FPSTHREAD *prm = (FPSTHREAD*)(argv.sival_ptr);
	pthread_mutex_lock(prm->mutex_data);
	++(prm->ticks);
	stdscr->_clear = true;
	prm->below->_clear = true;
	pthread_mutex_unlock(prm->mutex_data);

	draw_window(prm);
	//return NULL;
}

void draw_map(WINDOW *wnd, BCSMAP *map) {
	
}

void draw_window(FPSTHREAD *prm) {
	pthread_mutex_lock(prm->mutex_frame);
	if(stdscr->_clear) {
		stdscr->_clear = false;
		nassert(werase(stdscr));
		if (prm->map->map_primitives != NULL) {
			draw_map(stdscr, prm->map);
		}
		else {
			// do not check return value because text may be longer than window width
			mvwaddstr(stdscr, 0, 0, "Map is loading.");
			pthread_mutex_lock(prm->mutex_data);
			prm->delay_val = (prm->ticks / 15) % 6;
			pthread_mutex_unlock(prm->mutex_data);
			for(int i = 0; i < prm->delay_val; i++) {
				waddch(stdscr, '.');
			}
		}

		// draw player
		nassert(wmove(stdscr, prm->me_y, prm->me_x));
		wattron(stdscr, COLOR_PAIR(CPAIR_PLAYER_SELF));
		waddch(stdscr, ' ');
		wattroff(stdscr, COLOR_PAIR(CPAIR_PLAYER_SELF));
	}

	if(prm->below->_clear) {
		prm->below->_clear = false;
		nassert(werase(prm->below));
		pthread_mutex_lock(prm->mutex_data);
		mvwprintw(prm->below, 0, 1, "Frames: %zu", prm->ticks);
		pthread_mutex_unlock(prm->mutex_data);
	}

	nassert(wnoutrefresh(stdscr));
	nassert(wnoutrefresh(prm->below));
	nassert(doupdate());
	pthread_mutex_unlock(prm->mutex_frame);
}

int main(int argc, char **argv) {
	/////////////////////////
	//    ИНИЦИАЛИЗАЦИЯ    //
	/////////////////////////

	// TODO: быстрое подключение через командную строку

	char buf[DGRAM_MAX];
	// узнать все пригодные интерфейсы
	VECTOR ifaces;
	size_t iface_count = init_broadcast_receiver(&ifaces);

	int *ubcls = (int*)malloc(sizeof(int) * ifaces.size);

	int epollfd;
	struct epoll_event ev_catch;
	//int reuse_port = 1;
	__syscall(epollfd = epoll_create1(0));
	for(uint i = 0; i < iface_count; i++) {
		struct sockaddr_in sin = {
			  .sin_addr.s_addr = ((BCAST_UN*)(&(ifaces.array[i])))->v4.bcast
			, .sin_port = htons(DEFAULT_PORT + 1)
			, .sin_family = AF_INET
		};
		__syscall(ubcls[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
		//__syscall(setsockopt(ubcls[i], SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)));
		__syscall(bind(ubcls[i], (struct sockaddr*)&sin, sizeof(sin)));

		struct epoll_event evt = {
			  .events = EPOLLIN | EPOLLERR
			, .data.fd = ubcls[i]
		};
		__syscall(epoll_ctl(epollfd, EPOLL_CTL_ADD, ubcls[i], &evt));
	}

	VECTOR servers;
	lassert(vector_init(&servers, 10)); // TODO: get rid of magic number

	struct timeval tv_last, tv_now, tv_diff;
	__syscall(gettimeofday(&tv_last, NULL));
	ALOGI("Scanning for servers, please wait...\n");
	while(true) {
		int ret;
		__syscall(ret = epoll_wait(epollfd, &ev_catch, 1, 5000));
		if(ret == 0) {
			ALOGI("No broadcast from local servers\n");
			// TODO: ask for endpoint
			//break;
			exit(1);
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
			ssize_t len = recvfrom(ev_catch.data.fd, buf, DGRAM_MAX, 0
				, (struct sockaddr*)&srv_sin, &sa_len);
			BCSBEACON *beacon = (BCSBEACON*)buf;
			if(beacon->magic != htobe64(BCSBEACON_MAGIC)) {
				ALOGW("Received beacon magic %016lx != %016lx incorrect, skipping\n"
					, beacon->magic, (uint64_t)htobe64(BCSBEACON_MAGIC));
				continue;
			}

			if(len < (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon length is too short, skipping\n");
				continue;
			}

			if(len > (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon length is too long\n");
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
			char addrstr[INET_ADDRSTRLEN];
			lassert(inet_ntop(AF_INET, &srv_sin.sin_addr, addrstr, INET_ADDRSTRLEN));
			printf("\t%s:%hu\t%.*s\n"
				, addrstr, beacon->port, BCSBEACON_DESCRLEN, beacon->description);
			__syscall(gettimeofday(&tv_last, NULL));
			continue;
		}
next_epevent:
		__syscall(gettimeofday(&tv_now, NULL));
		timersub(&tv_now, &tv_last, &tv_diff);
		if(tv_diff.tv_sec >= 5) {
			ALOGI("No more servers detected (2)\n");
			break;
		}
	}
	ALOGI("Press Enter to continue... ");
	getchar();

	// TODO: close b/cast listeners there?
	for(size_t i = 0; i < iface_count; i++) {
		close(ubcls[i]);
	}
	free(ubcls);

	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutex_frame = PTHREAD_MUTEX_INITIALIZER;
	BCSMAP map = {
		  .width = 80
		, .height = 24
		, .map_primitives = NULL
	};
	srand(time(NULL));

	// NCurses
	nassert(initscr());
	nassert(raw());
	nassert(keypad(stdscr, true));
	nassert(curs_set(false));
	nassert(noecho());

	nassert(start_color());
	nassert(init_pair(CPAIR_CELL_BLANK, COLOR_WHITE, COLOR_BLUE)); // empty cell
	nassert(init_pair(CPAIR_PLAYER_SELF, COLOR_WHITE, COLOR_GREEN)); // me
	nassert(init_pair(CPAIR_PLAYER_ENEMY, COLOR_WHITE, COLOR_RED)); // opposite

	// 2D geometry
	int wnd_ymax, wnd_xmax;
	getmaxyx(stdscr, wnd_ymax, wnd_xmax);

	WINDOW *below;
	nassert(below = newwin(1, wnd_xmax / 2, wnd_ymax - 2, 1));
	nassert(wbkgd(below, COLOR_PAIR(CPAIR_DEFAULT)));

	FPSTHREAD prm = {
		  .map = &map
		, .mutex_data = &mutex
		, .mutex_frame = &mutex_frame
		, .ticks = 0
		, .delay_val = 0
		, .below = below
		, .me_x = 42 + rand() % 16
		, .me_y = 14 + rand() % 10
	};

	// запуск таймера
	struct sigevent sgv = {
		  .sigev_notify = SIGEV_THREAD
		, .sigev_value = { .sival_ptr = &prm }
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
		draw_window(&prm);

		/////////////////////////
		//         ВВОД        //
		/////////////////////////
		int64_t key = raw_wgetch(stdscr);
		pthread_mutex_lock(&mutex);
		switch(key) {
			/////////////////////////
			//       ОБРАБОТКА     //
			/////////////////////////

			case 0x3: // Ctrl+C
			case KEY_F(10): // F10
				goto loop_leave;

			case KEY_LEFT: prm.me_x = max(0, prm.me_x - 1); break;
			case KEY_RIGHT: prm.me_x = min(wnd_xmax - 1, prm.me_x + 1); break;

			case KEY_UP: prm.me_y = max(0, prm.me_y - 1); break;
			case KEY_DOWN: prm.me_y = min(wnd_ymax - 1, prm.me_y + 1); break;

			default: break;
		}
		pthread_mutex_unlock(&mutex);
	}

loop_leave:
	curs_set(true);
	endwin();

	return 0;
}
