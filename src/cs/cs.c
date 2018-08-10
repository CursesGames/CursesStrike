// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <ncursesw/ncurses.h>

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>
#include <netdb.h>
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
#include "../libbcsstatemachine/bcsstatemachine.h"
#include "../libbcsgameplay/bcsgameplay.h"

// catching broadcast messages timeout
#define BCAST_SCAN_TIMEOUT_SEC 4
// assume no more than 3 good ifaces by default
#define BCSIFACES_APPROX 3
// assume no more than 10 good servers by default
#define BCSSERVERS_APPROX 10
// connect datagrams will be resent this times
#define BCSCONNECT_RETRIES 10

#define STATS_WIDTH (BCSPLAYER_NICKLEN + 5 + 5 + 2)
#define STATS_HEIGHT (BCSSERVER_MAXCLIENTS + 2)

#define MAPVIEW_WIDTH 80
#define MAPVIEW_HEIGHT 24

// WARNING: this is a workaround for dumb C libraries like musl
typedef union sigval sigval_t;

// taken from Bionicle Commander
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
//	, BCSDIR_UP
//	, BCSDIR_RIGHT
//	, BCSDIR_DOWN
//} BCSDIRECTION;
const char dirchar[] = { '<', '^', '>', 'v' };

typedef BCSBEACON BCAST_SRV;

void draw_window(BCSPLAYER_FULL_STATE *pfs);

// Создаёт вектор интерфейсов. Вектор должен быть неинициализированным
size_t init_broadcast_receiver(VECTOR *ipv4_faces) {
	char addrstr[INET_ADDRSTRLEN];
	in_addr_t ifaddr;
	BCAST_UN un;
	size_t count = 0;
	struct ifaddrs *ifap_head;
	__syswrap(getifaddrs(&ifap_head));
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
			VECTOR_VALTYPE vval = { .lng = un._vval };
			lassert(vector_push_back(ipv4_faces, vval));
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
	BCSPLAYER_FULL_STATE *pfs = (BCSPLAYER_FULL_STATE*)argv;
	int sock = pfs->sockfd;
	struct __endpoint server = {
		  .addr = pfs->endpoint.sin_addr.s_addr
		, .port = pfs->endpoint.sin_port
		, .zero = 0
	};
	struct sockaddr_in src;
	socklen_t sa_len = sizeof(src);
	char buf[BCSDGRAM_MAX];

	while(true) {
		bool need_redraw = true;
		ssize_t rcvd = recvfrom(sock, buf, BCSDGRAM_MAX, 0, (sockaddr*)&src, &sa_len);
		if(rcvd == -1) {
			if (errno == EAGAIN) {
				// no announces from server
				goto force_redraw;
			}
			else
				__syswrap(-1);
		}
		if(src.sin_addr.s_addr != server.addr || src.sin_port != server.port) {
			ALOGD("Wrong datagram source: %s:%hu != %s:%hu\n"
				, inet_ntoa(src.sin_addr), be16toh(src.sin_port)
				, inet_ntoa(*(struct in_addr*)&(server.addr)), be16toh(server.port)
			);
			continue;
		}

		// на время обработки принятого сообщения блокируем состояние
		pthread_mutex_lock(&pfs->mutex_self);
		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		switch(be32toh(repl->type)) {
			case BCSREPLT_ANNOUNCE:
				// this is to trick the compiler that's not a declaration (:
				// ReSharper disable once CppAssignedValueIsNeverUsed
				// ReSharper disable once CppIdenticalOperandsInBinaryExpression
				rcvd = rcvd;
				BCSMSGANNOUNCE *ann = (BCSMSGANNOUNCE*)(repl + 1);
				BCSCLIENT_PUBLIC *players = (BCSCLIENT_PUBLIC*)(ann + 1);
			
				pfs->others.index_self = be16toh(ann->index_self);
				pfs->others.count = be16toh(ann->count);

				BCSBULLET *array_bullet = (BCSBULLET*)(players + pfs->others.count);
				//pthread_mutex_lock(&pfs->mutex_self);
				// copy self state
				//pfs->self = players[be16toh(ann->index_self)];
				memcpy(pfs->others.array, players, sizeof(BCSCLIENT_PUBLIC) * pfs->others.count);
				// convert to host from BE
				for(size_t i = 0; i < pfs->others.count; ++i) {
					pfs->others.array[i].direction = be32toh(pfs->others.array[i].direction);
					pfs->others.array[i].state = be32toh(pfs->others.array[i].state);
					pfs->others.array[i].position.x = be16toh(pfs->others.array[i].position.x);
					pfs->others.array[i].position.y = be16toh(pfs->others.array[i].position.y);
				}
				//pfs->self = players[be16toh(ann->index_self)];
				pfs->self = pfs->others.array[pfs->others.index_self];
				// copy bullets
				pfs->bullets.count = be16toh(ann->count_bullets);
				if(pfs->bullets.array != NULL)
					free(pfs->bullets.array);
				pfs->bullets.array = (BCSBULLET*)malloc(sizeof(BCSBULLET) * pfs->bullets.count);
				memcpy(pfs->bullets.array, array_bullet, sizeof(BCSBULLET) * pfs->bullets.count);
				// convert to host from BE
				for(size_t i = 0; i < pfs->bullets.count; ++i) {
					pfs->bullets.array[i].creator_id = be16toh(pfs->bullets.array[i].creator_id);
					pfs->bullets.array[i].direction = be32toh(pfs->bullets.array[i].direction);
					pfs->bullets.array[i].x = be16toh(pfs->bullets.array[i].x);
					pfs->bullets.array[i].y = be16toh(pfs->bullets.array[i].y);
				}
				__syswrap(gettimeofday(&pfs->last_announce, NULL));
				//pthread_mutex_unlock(&pfs->mutex_self);
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
				ALOGI("Received stats\n");
				// this is to trick the compiler that's not a declaration (:
				// ReSharper disable once CppAssignedValueIsNeverUsed
				// ReSharper disable once CppIdenticalOperandsInBinaryExpression
				rcvd = rcvd;
				ann = (BCSMSGANNOUNCE*)(repl + 1);
				BCSCLIENT_PUBLIC_EXT *plstats = (BCSCLIENT_PUBLIC_EXT*)(ann + 1);
			
				pfs->others.index_self = be16toh(ann->index_self);
				pfs->others.count = be16toh(ann->count);

				memcpy(pfs->others.stats, plstats, sizeof(BCSCLIENT_PUBLIC_EXT) * pfs->others.count);
				for(uint16_t i = 0; i < pfs->others.count; ++i) {
					pfs->others.stats[i].frags = be16toh(pfs->others.stats[i].frags);
					pfs->others.stats[i].deaths = be16toh(pfs->others.stats[i].deaths);
				}
				pfs->stats->_clear = true;
			break;

			case BCSREPLT_SHUTDOWN:
				// don't quit until frame is not drawn
				pthread_mutex_lock(&pfs->mutex_frame);
				curs_set(true);
				endwin();
				printf(ANSI_COLOR_RED "Server shutted down" ANSI_CLRST "\n");
				exit(2);
			break;

			case BCSREPLT_NONE: 
				ALOGW("Server sent message of type = %u, do nothing\n", be32toh(repl->type));
			break;
			default: __syswrap(-1);
		}
		pthread_mutex_unlock(&pfs->mutex_self);

force_redraw:
		if(need_redraw) {
			draw_window(pfs);
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
					// we want to leave this cell transparent
					//nassert(mvwinsch(pad, i, j, ' '));
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

void do_action(BCSPLAYER_FULL_STATE *pfs, BCSACTION action, BCSDIRECTION dir) {
	BCSMSG msg = {
		  .action = htobe32(action)
		, .un.long_p = 0
	};


	pthread_mutex_lock(&pfs->mutex_self);
	BCSCLST state = pfs->self.state;
	pthread_mutex_unlock(&pfs->mutex_self);

	switch(action) {
		case BCSACTION_DISCONNECT: break;
		case BCSACTION_MOVE: {
			// не спамим на сервер если ничё не можем
			if(state != BCSCLST_CONNECTED && state != BCSCLST_PLAYING)
				return;
			msg.un.ints.int_lo = htobe32(dir);
		} break;
		case BCSACTION_FIRE: {
			// не спамим на сервер если ничё не можем
			if(state != BCSCLST_CONNECTED && state != BCSCLST_PLAYING)
				return;
		} break;
		case BCSACTION_ROTATE: {
			// не спамим на сервер если ничё не можем
			if(state != BCSCLST_PLAYING)
				return;
			switch(dir) {
				case BCSDIR_LEFT:
				case BCSDIR_RIGHT:
					msg.un.ints.int_lo = htobe32(dir);
				break;

				default: ALOGW("Wrong direction for rotation: %u\n", dir); return;
			}
		} break;
		case BCSACTION_REQSTATS: {
			pthread_mutex_lock(&pfs->mutex_self);
			pfs->show_stats = !pfs->show_stats;
			bool request_stats = pfs->show_stats;
			pthread_mutex_unlock(&pfs->mutex_self);
			if(!request_stats)
				return;
		} break;

		default: ALOGW("Wrong action type %u\n", action); return;
	}

	bcsproto_new_packet(&msg);
	pthread_mutex_lock(&pfs->mutex_self);
	sendto(pfs->sockfd, &msg, sizeof(msg), 0, (sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
	pthread_mutex_unlock(&pfs->mutex_self);
}

void draw_window(BCSPLAYER_FULL_STATE *pfs) {
	//char buf[256];
	pthread_mutex_lock(&pfs->mutex_frame);
    
    int w, h;
	getmaxyx(stdscr, h, w);

	// блокируем этот мьютекс только в случае чтения изменяющихся данных или записи изменений
	pthread_mutex_lock(&pfs->mutex_self);
	size_t frames = ++pfs->frames;
	BCSCLIENT_PUBLIC self = pfs->self;
	BCSMAP map = pfs->map;
	WINDOW *mappad = pfs->mappad;
	WINDOW *mapobj = pfs->mapobj;
	//WINDOW *below = pfs->below;
	WINDOW *stats = pfs->stats;

	// стата
	bool show_stats = pfs->show_stats;
	uint16_t others_count = pfs->others.count;
	//pthread_mutex_unlock(&pfs->mutex_self);

	// repaint from scratch every 30 frames?
	if((frames % 60) == 0)
		nassert(clearok(stdscr, true));
	//nassert(touchwin(stdscr));
	//nassert(touchwin(mapobj));
	nassert(werase(stdscr));
	nassert(werase(mapobj));

	// draw players
	if(self.state == BCSCLST_PLAYING) {
		nassert(wattron(mapobj, COLOR_PAIR(CPAIR_PLAYER_SELF)));
		nassert(mvwputch(mapobj, self.position.y, self.position.x, dirchar[self.direction]));
		nassert(wattroff(mapobj, COLOR_PAIR(CPAIR_PLAYER_SELF)));
	}

	// lock for all enemies, I don't think this is slow
	//pthread_mutex_lock(&pfs->mutex_self);
	for(uint16_t i = 0; i < pfs->others.count; i++) {
		if(i != pfs->others.index_self && pfs->others.array[i].state == BCSCLST_PLAYING) {
			BCSCLIENT_PUBLIC enemy = pfs->others.array[i];
			nassert(wattron(mapobj, COLOR_PAIR(CPAIR_PLAYER_ENEMY)));
			nassert(mvwputch(mapobj, enemy.position.y, enemy.position.x, dirchar[enemy.direction]));
			nassert(wattroff(mapobj, COLOR_PAIR(CPAIR_PLAYER_ENEMY)));
		}
	}

	for(uint16_t i = 0; i < pfs->bullets.count; ++i) {
		if(pfs->bullets.array[i].y < h && pfs->bullets.array[i].x < w) {
			uint16_t bx = pfs->bullets.array[i].x;
			uint16_t by = pfs->bullets.array[i].y;
			// check for glitches
			BCSMAPPRIMITIVE prim = pfs->map.map_primitives[by * map.width + bx];
			if(prim != PUNIT_OPEN_SPACE && prim != PUNIT_WATER)
				continue;

			char c = ' ';
			switch(pfs->bullets.array[i].direction) {
				case BCSDIR_LEFT:
				case BCSDIR_RIGHT:
					c = '-';
				break;

				case BCSDIR_UP:
				case BCSDIR_DOWN:
					c = '|';
				break;
			}
			nassert(mvwputch(mapobj, by, bx, c));
		}
	}

	// redraw stats if we need it
	if(stats->_clear) {
		stats->_clear = false;
		nassert(werase(stats));
		nassert(wresize(stats, others_count + 2, getmaxx(stats)));
		nassert(wborder(stats, '|', '|', '=', '=', '#', '#', '#', '#'));
		for(uint16_t i = 0; i < pfs->others.count; ++i) {
			nassert(mvwaddattrfstr(stats, i + 1, 1, BCSPLAYER_NICKLEN
				, pfs->others.stats[i].nickname, A_NORMAL));
			nassert(mvwprintw(stats, i + 1, BCSPLAYER_NICKLEN + 1, "%4u", pfs->others.stats[i].frags));
			nassert(mvwprintw(stats, i + 1, BCSPLAYER_NICKLEN + 6, "%4u", pfs->others.stats[i].deaths));
		}
	}

	if(pfs->smooth_bullets)
		nassert(mvwaddstr(stdscr, h - 1, 0, "SMOOTH"));
	pthread_mutex_unlock(&pfs->mutex_self);

	// copy a part of pad
	// первые два параметра - верхний левый угол pad, с которого берём
	// следующие четыре - куда на экран проецируем
	//nassert(touchwin(mappad));
	//nassert(touchwin(mapobj));
	// фиксим размеры: пришло время
	int size_y = min(MAPVIEW_HEIGHT, min(h + 1, map.height));
	int size_x = min(MAPVIEW_WIDTH, min(w + 1, map.width));

	//nassert(pnoutrefresh(mappad, 0, 0, 0, 0, 0 + h, 0 + w));
	nassert(copywin(mappad, stdscr, 0, 0, 0, 0, size_y - 1, size_x - 1, true));

	if(show_stats) {
		//pnoutrefresh(stats, 0, 0, 0, 0, STATS_HEIGHT, STATS_WIDTH);
		nassert(copywin(stats, stdscr, 0, 0, 0, 0, others_count + 1, STATS_WIDTH - 1, false));
	}

	nassert(copywin(mapobj, stdscr, 0, 0, 0, 0, size_y - 1, size_x - 1, true));
	//nassert(pnoutrefresh(mapobj, 0, 0, 0, 0, 0 + h, 0 + w));
	//nassert(wnoutrefresh(mapobj));

	// эта панелька наложится поверх карты
	//sprintf(buf, "Frames: %zu", frames);
	//nassert(mvwaddstr(below, 0, 1, buf));
	//nassert(overlay(below, stdscr));

	nassert(wnoutrefresh(stdscr));

	// всё готово, можно слать клиенту пачку данных
	nassert(doupdate());
	// порисовали и хватит
	pthread_mutex_unlock(&pfs->mutex_frame);
}

// это отдельный поток
void *smooth_bullets(void *argv) {
	BCSPLAYER_FULL_STATE *pfs = (BCSPLAYER_FULL_STATE*)argv;
	timeval128_t now, last = { .tv_sec = 0, .tv_usec = 0 }, diff;
	timeval128_t do_it = {
		  .tv_sec = 0
		, .tv_usec = 11111
	};
	
	bool need_refresh = false;
	bool do_smoothing;
	while(true) {
		diff.tv_usec = do_it.tv_usec;
		pthread_mutex_lock(&pfs->mutex_self);
		do_smoothing = pfs->smooth_bullets;
		// если нет пуль то нечего и сглаживать
		if(pfs->bullets.count == 0)
			goto unlock;
		// если пришёл новый анонс, нужно обновить данные
		if(timercmp(&pfs->last_announce, &last, >)) {
			last = pfs->last_announce;
		}
		__syswrap(gettimeofday(&now, NULL));
		timersub(&now, &last, &diff);
		if(timercmp(&diff, &do_it, >=)) {
			// двигаем пульки
			for(uint16_t i = 0; i < pfs->bullets.count; ++i) {
				switch(pfs->bullets.array[i].direction) {
					case BCSDIR_LEFT: --(pfs->bullets.array[i].x); break;
					case BCSDIR_UP: --(pfs->bullets.array[i].y); break;
					case BCSDIR_RIGHT: ++(pfs->bullets.array[i].x); break;
					case BCSDIR_DOWN: ++(pfs->bullets.array[i].y); break;
				}
			}
			// думаю, это нужно
			timeval128_t tmp;
			timeradd(&last, &do_it, &tmp);
			last = tmp;
			need_refresh = true;
			ALOGD("Smoothed\n");
		}
unlock:
		pthread_mutex_unlock(&pfs->mutex_self);
		if(!do_smoothing) {
			sleep(1);
			continue;
		}

		if (need_refresh) {
			draw_window(pfs);
			need_refresh = false;
		}
		else {
			// выжидаем более удачный момент
			usleep(diff.tv_usec);
		}
	}
	
	// ReSharper disable once CppUnreachableCode
	return NULL;
}

int main(int argc, char **argv) {
	/////////////////////////
	//    ИНИЦИАЛИЗАЦИЯ    //
	/////////////////////////

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
			  .state = BCSCLST_STANDALONE
			, .direction = BCSDIR_UP
			, .position= {
				  .x = 0
				, .y = 0
			  }
		  }
		, .self_ext = {
			  .frags = 0
			, .deaths = 0
			, .nickname = "Unknown Soldier"
		  }
		, .others = {
			  .count = 0
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
		, .bullets = {
			  .array = NULL
			, .count = 0
		  }
		, .mappad = NULL
		, .mapobj = NULL
		, .below = NULL
		, .stats = NULL
		, .frames = 0
		, .endpoint = {
			  .sin_addr = INADDR_ANY // addr is in hE
			, .sin_port = BCSSERVER_DEFAULT_PORT // port is always in hE
			, .sin_family = AF_INET
		  }
		, .sockfd = -1 // erroneous socket
		, .redraw = true
		, .show_stats = false
		, .smooth_bullets = true
	};

	char buf[BCSDGRAM_MAX];
	char addrstr[INET_ADDRSTRLEN];

	if(getlogin_r(pfs.self_ext.nickname, BCSPLAYER_NICKLEN) != 0) {
		if (getenv("LOGNAME") != NULL)
			strncpy(pfs.self_ext.nickname, getenv("LOGNAME"), BCSPLAYER_NICKLEN);
		else
			//strncpy(pfs.self_ext.nickname, cuserid(NULL), BCSPLAYER_NICKLEN);
			ALOGW("Could not determine your username\n");
	}

	if(argc > 1) {
		// try to get endpoint from argv[1]
		// taken from: SibSutis\labs_ptid_self\rgz_ptid_self\program.cpp
		char *ptr = argv[1];
		while (*ptr != ':' && *ptr != '\0') ++ptr;
		if (*ptr == ':') {
			if (sscanf(ptr + 1, "%hu", &pfs.endpoint.sin_port) < 1) {
				ALOGE("Incorrect port number\n");
				return EXIT_FAILURE;
			}
			//pfs.endpoint.sin_port = pfs.endpoint.sin_port;
			*ptr = '\0'; // WARNING: now we patch argv
		}

		if (!inet_pton(AF_INET, argv[1], &pfs.endpoint.sin_addr)) {
			// Resolve hostname
			struct hostent *sv_host = gethostbyname(argv[1]);
			if (!sv_host) {
				ALOGE("Could not resolve hostname\n");
				return EXIT_FAILURE;
			}

			size_t ip_count;
			// ReSharper disable once CppPossiblyErroneousEmptyStatements
			for (ip_count = 0; sv_host->h_addr_list[ip_count] != NULL; ++ip_count);
			// DONE: select random IP if count > 1, up to RAND_MAX, e.g. 32767
			pfs.endpoint.sin_addr = *(struct in_addr*)(sv_host->h_addr_list[rand() % ip_count]);
		}
		//pfs.endpoint.sin_addr.s_addr = be32toh(pfs.endpoint.sin_addr.s_addr);
		goto connect_to;
	}
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
	__syswrap(epollfd = epoll_create1(0));
	for(uint i = 0; i < iface_count; i++) {
		struct sockaddr_in sin = {
			  .sin_addr.s_addr = ((BCAST_UN*)(&(ifaces.array[i])))->v4.bcast
			, .sin_port = htobe16(BCSSERVER_BCAST_PORT)
			, .sin_family = AF_INET
		};
		__syswrap(ubcls[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
		// reuse addr to allow server & client on the same iface
		__syswrap(setsockopt(ubcls[i], SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)));
		__syswrap(bind(ubcls[i], (sockaddr*)&sin, sizeof(sin)));

		struct epoll_event evt = {
			  .events = EPOLLIN | EPOLLERR
			, .data.fd = ubcls[i]
		};
		__syswrap(epoll_ctl(epollfd, EPOLL_CTL_ADD, ubcls[i], &evt));
	}

	vector_free(&ifaces);

	VECTOR servers;
	lassert(vector_init(&servers, BCSSERVERS_APPROX));
	uint32_t number = 0;
	timeval128_t tv_last, tv_now, tv_diff;
	__syswrap(gettimeofday(&tv_last, NULL));

	ALOGI("Scanning for servers, please wait...\n");
	while(true) {
		int ret;
		__syswrap(ret = epoll_wait(epollfd, &ev_catch, 1, BCAST_SCAN_TIMEOUT_SEC * 1000));
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
					, .port = be16toh(beacon->port)
					, .zero = 0
				}
			};

			for(size_t i = 0; i < servers.size; i++) {
				if(servers.array[i].lng == srv_new._vval) {
					// this server is already listed
					goto next_epevent;
				}
			}

			VECTOR_VALTYPE vval = { .lng = srv_new._vval };
			lassert(vector_push_back(&servers, vval));
			lassert(inet_ntop(AF_INET, &srv_sin.sin_addr, addrstr, INET_ADDRSTRLEN));
			printf("\t%s:%hu\t%.*s - %u\n"
				, addrstr, srv_new.endpoint.port, BCSBEACON_DESCRLEN, beacon->description
				, number + 1
			);
			++number;
			__syswrap(gettimeofday(&tv_last, NULL));
			continue;
		}
next_epevent:
		__syswrap(gettimeofday(&tv_now, NULL));
		timersub(&tv_now, &tv_last, &tv_diff);
		if(tv_diff.tv_sec >= BCAST_SCAN_TIMEOUT_SEC) {
			ALOGI("Server scan finished\n");
			break;
		}
	}

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
		if(fgets(buf, 256, stdin) == NULL)
			goto connect_leave;
		if(buf[0] == '\n')
			break;
		if(sscanf(buf, "%u", &idx) == 1 && idx <= servers.size)
			break;
		printf("Sorry you can't!\n");
	}

	if(idx == 0) {
		vector_free(&servers);
		goto start_bcast_scan; // FIXME
	}
	
	// connect to selected server
	BCAST_SRV_UN *srv = (BCAST_SRV_UN*)(&(servers.array[idx - 1]));
	pfs.endpoint.sin_addr.s_addr = srv->endpoint.addr;
	pfs.endpoint.sin_port = srv->endpoint.port;

connect_to:
	lassert(inet_ntop(AF_INET, &(pfs.endpoint.sin_addr), addrstr, INET_ADDRSTRLEN) != NULL);
	ALOGI("Connecting to %s:%hu\n", addrstr, pfs.endpoint.sin_port);

	// inverse port once
	pfs.endpoint.sin_port = htobe16(pfs.endpoint.sin_port);

	VERBOSE ALOGV("Creating UDP socket... ");
	__syswrap(pfs.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
	timeval128_t rcvtimeo = {
		  .tv_sec = (BCSRECV_TIMEO / 1000)
		, .tv_usec = (BCSRECV_TIMEO % 1000)
	};
	__syswrap(setsockopt(pfs.sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)));
	VERBOSE logprint("OK.\n");

	// есть смысл сначала создать структуру, а только потом проштамповать
	// зачем? чтобы метка времени создания была ближе к времени отправки
	ssize_t nickname_len = strlen(pfs.self_ext.nickname);
	ALOGV("Nickname length = %zd\n", nickname_len);
	ssize_t connect_len = sizeof(BCSMSG) + nickname_len + 1;
	ALOGV("Connect length = %zd\n", connect_len);
	
	BCSMSG *msg = alloca(connect_len);
	msg->action = htobe32(BCSACTION_CONNECT);
	msg->un.ints.int_lo = htobe32(BCSPROTO_VERSION);
	msg->un.ints.int_hi = htobe32(nickname_len);
	strcpy((char*)(msg + 1), pfs.self_ext.nickname);

	VERBOSE ALOGV("Sending CONNECT... ");

	ssize_t rcvd;
	struct sockaddr_in sin_src;
	socklen_t src_alen = sizeof(sin_src);
	size_t retries = 0;
	while(true) {
		if (retries >= BCSCONNECT_RETRIES) {
			VERBOSE logprint("\n");
			ALOGE("Connection failed: server does not respond\n");
			goto connect_leave;
		}

		bcsproto_new_packet(msg);
		lassert(sendto(pfs.sockfd, msg, connect_len, 0
			, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)) == connect_len);
		rcvd = recvfrom(pfs.sockfd, buf, BCSDGRAM_MAX, 0, (sockaddr*)&sin_src, &src_alen);
		if(rcvd == -1) {
			if (errno == EAGAIN) {
				VERBOSE logprint("#");
				++retries;
				continue;
			}
			else
				__syswrap(-1);
		}

		if(    sin_src.sin_addr.s_addr != pfs.endpoint.sin_addr.s_addr
			|| sin_src.sin_port        != pfs.endpoint.sin_port
		) {
			ALOGD("Source endpoint mismatch, skipping\n");
			sleep(1);
			continue;
		}

		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		if(repl->packet_no != msg->packet_no) {
			ALOGD("Packet no mismatch (sent %u, got %u), skipping\n"
				, be32toh(msg->packet_no), be32toh(repl->packet_no));
			continue;
		}

		if(be32toh(repl->type) != BCSREPLT_MAP) {
			ALOGE("Reply type mismatch (expecting %u, got %u), exiting\n"
				, BCSREPLT_MAP, be32toh(repl->type));
			goto connect_leave;
		}

		// if we are there: source matches, packet no and reply type too
		VERBOSE logprint("OK.\n");
		break;
	}

	// буду строг к себе в соблюдении состояний
	// ReSharper disable once CppAssignedValueIsNeverUsed
	pfs.self.state = BCSCLST_CONNECTING;
	VERBOSE ALOGV("Server told to download the map.\n");
	VERBOSE ALOGD("Connecting to the TCP socket... ");
	int sock_tcp;
	__syswrap(sock_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	__syswrap(connect(sock_tcp, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)));
	VERBOSE logprint("OK.\n");

	VERBOSE ALOGV("Receiving map params... ");
	__syswrap(recv(sock_tcp, &pfs.map, 4, MSG_WAITALL)); // FIXME: magic const 4 is a sizeof (w+h)
	pfs.map.width = be16toh(pfs.map.width);
	pfs.map.height = be16toh(pfs.map.height);
	VERBOSE logprint("%hux%hu\n", pfs.map.width, pfs.map.height);

	ssize_t size_in_bytes = pfs.map.width * pfs.map.height;
	pfs.map.map_primitives = (uint8_t*)malloc(size_in_bytes);
	VERBOSE ALOGV("Receiving map data... ");
	__syswrap(recv(sock_tcp, pfs.map.map_primitives, size_in_bytes, MSG_WAITALL));
	VERBOSE logprint("%lu bytes OK.\n", size_in_bytes);

	close(sock_tcp);

	msg->action = htobe32(BCSACTION_CONNECT2);
	VERBOSE ALOGV("Sending CONNECT2... ");

	while(true) {
		bcsproto_new_packet(msg);
		lassert(sendto(pfs.sockfd, msg, sizeof(BCSMSG), 0
			, (sockaddr*)&pfs.endpoint, sizeof(pfs.endpoint)) == sizeof(BCSMSG));
		rcvd = recvfrom(pfs.sockfd, buf, BCSDGRAM_MAX, 0, (sockaddr*)&sin_src, &src_alen);
		if(rcvd == -1) {
			if (errno == EAGAIN) {
				VERBOSE logprint("#");
				++retries;
				if (retries >= BCSCONNECT_RETRIES) {
					VERBOSE logprint("\n");
					ALOGE("Connection failed: server does not respond\n");
					goto connect_leave;
				}
				continue;
			}
			else
				__syswrap(-1);
		}
		if(    sin_src.sin_addr.s_addr != pfs.endpoint.sin_addr.s_addr
			|| sin_src.sin_port        != pfs.endpoint.sin_port
		) {
			ALOGD("Source endpoint mismatch, skipping\n");
			continue;
		}

		BCSMSGREPLY *repl = (BCSMSGREPLY*)buf;
		if(repl->packet_no != msg->packet_no) {
			ALOGD("Packet no mismatch (sent %u, got %u), skipping\n"
				, be32toh(msg->packet_no), be32toh(repl->packet_no));
			continue;
		}

		if(be32toh(repl->type) != BCSREPLT_ACK) {
			ALOGE("Reply type mismatch (expecting %u, got %u), skipping\n"
				, BCSREPLT_ACK, be32toh(repl->type));
			goto connect_leave;
		}

		// if we are there: source matches, packet no and reply type too
		break;
	}
	VERBOSE logprint("OK.\n");

	// прелюдия закончена, мы должны быть на сервере.
	pfs.self.state = BCSCLST_CONNECTED;
	// перенаправляем лог в файл
	sassert(freopen("cs_client.log", "w", stderr) != NULL);

	// NCurses
	nassert(initscr());
	nassert(raw());
	nassert(keypad(stdscr, true));
	nassert(curs_set(false));
	nassert(noecho());
	idcok(stdscr, true);
	idcok(curscr, true);
	nassert(idlok(stdscr, false));
	nassert(idlok(curscr, false));
	nassert(leaveok(stdscr, true));
	nassert(leaveok(curscr, true));
	nassert(nonl());
	//nassert(use_default_colors()); // for transparency?

	nassert(start_color());
	nassert(init_pair(CPAIR_CELL_BLANK, COLOR_WHITE, COLOR_BLACK)); // empty cell
	nassert(init_pair(CPAIR_CELL_WALL, COLOR_BLACK, COLOR_WHITE)); // wall
	nassert(init_pair(CPAIR_CELL_CRATE, COLOR_BLACK, COLOR_YELLOW)); // crate
	nassert(init_pair(CPAIR_CELL_WATER, COLOR_WHITE, COLOR_BLUE)); // wall

	nassert(init_pair(CPAIR_PLAYER_SELF, COLOR_BLACK, COLOR_GREEN)); // me
	nassert(init_pair(CPAIR_PLAYER_ENEMY, COLOR_WHITE, COLOR_RED)); // opposite

	// 2D geometry
	int wnd_ymax, wnd_xmax;
	getmaxyx(stdscr, wnd_ymax, wnd_xmax);

	nassert(pfs.mapobj = newwin(wnd_ymax, wnd_xmax, 0, 0));
	idcok(pfs.mapobj, true);
	nassert(idlok(pfs.mapobj, false));
	nassert(leaveok(pfs.mapobj, false));

	nassert(pfs.below = newwin(1, wnd_xmax / 2, wnd_ymax - 2, 1));
	nassert(wbkgd(pfs.below, COLOR_PAIR(CPAIR_DEFAULT)));
	pfs.below->_clear = true;

	nassert(pfs.stats = newpad(STATS_HEIGHT, STATS_WIDTH));
	nassert(wbkgd(pfs.stats, COLOR_PAIR(CPAIR_CELL_WALL)));

	init_map(&pfs.mappad, &pfs.map);

	pthread_t thread_smoother;
	sassert(pthread_create(&thread_smoother, NULL, smooth_bullets, &pfs) == 0);

	pthread_t receiver_thread;
	sassert(pthread_create(&receiver_thread, NULL, receiver_func, &pfs) == 0);

	bool has_pressed = false;
	while(true) {
		/////////////////////////
		//      ОТРИСОВКА      //
		/////////////////////////
		//if(key != ERR) {
		if(has_pressed) {
			draw_window(&pfs);
		}

		/////////////////////////
		//         ВВОД        //
		/////////////////////////
		//nassert(nodelay(stdscr, true));
		int32_t key = raw_wgetch(stdscr);
		switch(key) {
			/////////////////////////
			//       ОБРАБОТКА     //
			/////////////////////////

			case 0x3: // Ctrl+C
			case KEY_F(10): // F10
				do_action(&pfs, BCSACTION_DISCONNECT, BCSDIR_UNDEF);
				goto loop_leave;

			case KEY_F(8): { // F8
				pthread_mutex_lock(&pfs.mutex_frame);
				FILE *f = fopen("stdscr.ncurses.log", "wb"); putwin(stdscr, f); fclose(f);
				f = fopen("curscr.ncurses.log", "wb"); putwin(curscr, f); fclose(f);
				f = fopen("newscr.ncurses.log", "wb"); putwin(newscr, f); fclose(f);
				f = fopen("mappad.ncurses.log", "wb"); putwin(pfs.mappad, f); fclose(f);
				f = fopen("mapobj.ncurses.log", "wb"); putwin(pfs.mapobj, f); fclose(f);
				pthread_mutex_unlock(&pfs.mutex_frame);
			} break;

			case KEY_F(7): { // F7
				pthread_mutex_lock(&pfs.mutex_self);
				pfs.smooth_bullets = !pfs.smooth_bullets;
				pthread_mutex_unlock(&pfs.mutex_self);
			} break;

			case RAW_KEY_TAB:
							do_action(&pfs, BCSACTION_REQSTATS, BCSDIR_UNDEF); 
																			 break;

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

			case ERR:
			default:
				has_pressed = false;
			break;
		}
		if (has_pressed) {
			usleep(75000);
			// skip key press queue
			//nodelay(stdscr, true);
			//while(true) {
			//	if(wgetch(stdscr) == ERR)
			//		break;
			//}
			//nodelay(stdscr, false);
			nassert(flushinp());
		}
	}

loop_leave:
	// don't quit until frame is not drawn
	pthread_mutex_lock(&pfs.mutex_frame);
	curs_set(true);
	endwin();

connect_leave:
	vector_free(&servers);

	return 0;
}
