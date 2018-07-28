#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdbool.h>
#include <stdint.h>
#include <ncursesw/ncurses.h>

// https://www.guyrutenberg.com/2008/12/20/expanding-macros-into-string-constants-in-c

#ifndef _MSC_VER
#define nassert(x)                                                            \
    {                                                                         \
        __typeof(x) y = (x);                                                  \
        if(__builtin_types_compatible_p(__typeof(y), WINDOW*)) {              \
            if((void*)(uintptr_t)(y) == (void*)(uintptr_t)NULL)               \
                ncurses_raise_error((#x), __FILE__, __LINE__);                \
        } else if(__builtin_types_compatible_p(__typeof(y), int)) {           \
            if((int)(intptr_t)(y) == ERR)                                     \
                ncurses_raise_error((#x), __FILE__, __LINE__);                \
        } else ncurses_raise_error("macro error in " #x, __FILE__, __LINE__); \
    }
// TODO: define nassert(x,str)
#else
#define nassert(x) (x)
#endif

#define custom_assert(x, y) (void)(!!(x) || y((#x), __FILE__, __LINE__))

// WARNING: evaluates twice
#define min(a,b) ((a) < (b) ? (a) : (b))
// WARNING: evaluates twice
#define max(a,b) ((a) > (b) ? (a) : (b))

#pragma GCC diagnostic ignored "-Wpedantic"
enum raw_keys {
	  RAW_KEY_ERR = -1
    , RAW_KEY_TAB = 9
    , RAW_KEY_ENTER = 10
    , RAW_KEY_NUMPAD_ENTER = 343
    , RAW_KEY_ESC = 27
    , RAW_KEY_HOME = 0x1b5b317e
    , RAW_KEY_HOME_ALT = 0x106
    , RAW_KEY_END = 0x1b5b347e
    , RAW_KEY_END_ALT = 0x168
    , RAW_KEY_PAGE_DOWN = 0x152
    , RAW_KEY_PAGE_UP = 0x153
	// long long key codes
	, RAW_KEY_F1 = 0x1b5b31317e
	, RAW_KEY_F2 = 0x1b5b31327e
	, RAW_KEY_F3 = 0x1b5b31337e
	, RAW_KEY_F4 = 0x1b5b31347e
};
#pragma GCC diagnostic warning "-Wpedantic"

bool ncurses_raise_error(const char *x, const char *file, int line);
chtype *create_chstr(char *str, int len, chtype attr);
int mvwaddattrfstr(WINDOW *wnd, int y, int x, int len, char *str, chtype attr);
int64_t raw_wgetch(WINDOW *wnd);
