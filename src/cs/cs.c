// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <ncursesw/ncurses.h>
#include <pthread.h>

#include "../libncurses_util/ncurses_util.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"

// taken from Bionicle Commander: https://github.com/Str1ker17/EltexLearning/blob/aa2fddc/src/bc/bc.c#L56
typedef enum {
	  CPAIR_DEFAULT = 1
	, CPAIR_PANEL
	, CPAIR_PANEL_TITLE
	, CPAIR_HIGHLIGHT
	, CPAIR_EXECUTABLE
	, CPAIR_TYPE_DIR = CPAIR_PANEL
	, CPAIR_TYPE_REG = CPAIR_PANEL
	, CPAIR_TYPE_SYMLINK = CPAIR_EXECUTABLE + 1
	, CPAIR_TYPE_CHR
	, CPAIR_TYPE_BLK
	, CPAIR_TYPE_FIFO
	, CPAIR_TYPE_SOCKET
	, CPAIR_PLAYER_SELF
	, CPAIR_PLAYER_FRIENDLY
	, CPAIR_PLAYER_ENEMY
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

void draw_window(FPSTHREAD *prm);

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
	//    »Õ»÷»¿À»«¿÷»ﬂ    //
	/////////////////////////
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutex_frame = PTHREAD_MUTEX_INITIALIZER;
	BCSMAP map = {
		  .width = 160
		, .height = 80
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
	nassert(init_pair(CPAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK)); // non-panel
	nassert(init_pair(CPAIR_PANEL, COLOR_WHITE, COLOR_BLUE)); // panel
	nassert(init_pair(CPAIR_PANEL_TITLE, COLOR_BLACK, COLOR_WHITE)); // panel title

	nassert(init_pair(CPAIR_PLAYER_SELF, COLOR_WHITE, COLOR_GREEN)); // me
	nassert(init_pair(CPAIR_PLAYER_FRIENDLY, COLOR_WHITE, COLOR_GREEN)); // team
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

	// Á‡ÔÛÒÍ Ú‡ÈÏÂ‡
	struct sigevent sgv = {
		  .sigev_notify = SIGEV_THREAD
		, .sigev_value = { .sival_ptr = &prm }
		, .sigev_signo = SIGALRM
		, .sigev_notify_function = timer_detonate
		, ._sigev_un._sigev_thread._attribute = NULL
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
		//      Œ“–»—Œ¬ ¿      //
		/////////////////////////
		draw_window(&prm);

		/////////////////////////
		//         ¬¬Œƒ        //
		/////////////////////////
		int64_t key = raw_wgetch(stdscr);
		pthread_mutex_lock(&mutex);
		switch(key) {

			/////////////////////////
			//       Œ¡–¿¡Œ“ ¿     //
			/////////////////////////

			case 0x3: // Ctrl+C
			case KEY_F(10):
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
