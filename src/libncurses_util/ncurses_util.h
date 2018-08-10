#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdbool.h>
#include <stdint.h>
#include <ncursesw/ncurses.h>
// ReSharper disable once CppUnusedIncludeDirective
#include "../liblinux_util/linux_util.h"

#ifndef _MSC_VER
// assert for any ncurses return value
#define nassert(x)                                                            \
    {                                                                         \
        __typeof(x) y = (x);                                                  \
        if(__builtin_types_compatible_p(__typeof(y), WINDOW*)) {              \
            if((void*)(uintptr_t)(y) == (void*)(uintptr_t)NULL)               \
                ncurses_raise_error(PASSSTR(x), __FILE__, __LINE__);          \
        } else if(__builtin_types_compatible_p(__typeof(y), int)) {           \
            if((int)(intptr_t)(y) == ERR)                                     \
                ncurses_raise_error(PASSSTR(x), __FILE__, __LINE__);          \
        } else ncurses_raise_error("macro error in " #x, __FILE__, __LINE__); \
    }
#else
#define nassert(x) (x)
#endif

enum raw_keys {
      RAW_KEY_ERR = -1
    , RAW_KEY_TAB = 9
    , RAW_KEY_ENTER = 10
    , RAW_KEY_NUMPAD_ENTER = 343
    , RAW_KEY_ESC = 27
    , RAW_KEY_HOME = 0x5b317e
    , RAW_KEY_HOME_ALT = 0x106
    , RAW_KEY_END = 0x5b347e
    , RAW_KEY_END_ALT = 0x168
    , RAW_KEY_PAGE_DOWN = 0x152
    , RAW_KEY_PAGE_UP = 0x153
      // long long key codes
    , RAW_KEY_F1 = 0x5b31317e
    , RAW_KEY_F2 = 0x5b31327e
    , RAW_KEY_F3 = 0x5b31337e
    , RAW_KEY_F4 = 0x5b31347e
};

// print error into stdout and stderr in form:
// [x] ncurses err: '<x>' at <file>: line
// calls endwin() and exit()
extern bool ncurses_raise_error(const char *x, const char *file, int line);

// create chtype string from char string with:
extern chtype *create_chstr(char *str, int len, chtype attr);

// move, add attributed string with fixed length:
extern int mvwaddattrfstr(WINDOW *wnd, int y, int x, int len, char *str, chtype attr);

// read from keyboard with long key codes
// возвращает 4-байтное число, в котором хранится длинный код символа
extern int32_t raw_wgetch(WINDOW *wnd);

// put char into the pointed by (x,y) position.
// uses addch() on all positions except for the right bottom corner;
// in the right bottom corner uses insch()
extern int mvwputch(WINDOW *wnd, int y, int x, chtype ch);
