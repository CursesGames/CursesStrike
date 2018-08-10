#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "mscfix.h"

#include <stdbool.h>

// Colored output
// https://stackoverflow.com/a/3219471/1543625
// Don't bother with libraries, the code is really simple.
#define ANSI_COLOR_BLACK          "\x1b[30m"
#define ANSI_COLOR_RED            "\x1b[31m"
#define ANSI_COLOR_GREEN          "\x1b[32m"
#define ANSI_COLOR_YELLOW         "\x1b[33m"
#define ANSI_COLOR_BLUE           "\x1b[34m"
#define ANSI_COLOR_MAGENTA        "\x1b[35m"
#define ANSI_COLOR_CYAN           "\x1b[36m"
#define ANSI_COLOR_WHITE          "\x1b[37m"

#define ANSI_COLOR_BRIGHT_BLACK   "\x1b[90m"
#define ANSI_COLOR_BRIGHT_RED     "\x1b[91m"
#define ANSI_COLOR_BRIGHT_GREEN   "\x1b[92m"
#define ANSI_COLOR_BRIGHT_YELLOW  "\x1b[93m"
#define ANSI_COLOR_BRIGHT_BLUE    "\x1b[94m"
#define ANSI_COLOR_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_COLOR_BRIGHT_CYAN    "\x1b[96m"
#define ANSI_COLOR_BRIGHT_WHITE   "\x1b[97m"

// https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
#define ANSI_BKGRD_BLACK          "\x1b[40m"
#define ANSI_BKGRD_RED            "\x1b[41m"
#define ANSI_BKGRD_GREEN          "\x1b[42m"
#define ANSI_BKGRD_YELLOW         "\x1b[43m"
#define ANSI_BKGRD_BLUE           "\x1b[44m"
#define ANSI_BKGRD_MAGENTA        "\x1b[45m"
#define ANSI_BKGRD_CYAN           "\x1b[46m"
#define ANSI_BKGRD_WHITE          "\x1b[47m"

#define ANSI_BKGRD_BRIGHT_BLACK   "\x1b[A0m"
#define ANSI_BKGRD_BRIGHT_RED     "\x1b[A1m"
#define ANSI_BKGRD_BRIGHT_GREEN   "\x1b[A2m"
#define ANSI_BKGRD_BRIGHT_YELLOW  "\x1b[A3m"
#define ANSI_BKGRD_BRIGHT_BLUE    "\x1b[A4m"
#define ANSI_BKGRD_BRIGHT_MAGENTA "\x1b[A5m"
#define ANSI_BKGRD_BRIGHT_CYAN    "\x1b[A6m"
#define ANSI_BKGRD_BRIGHT_WHITE   "\x1b[A7m"

#define ANSI_COLOR_RESET          "\x1b[0m"
#define ANSI_CLRST                ANSI_COLOR_RESET

// WARNING: evaluates twice
#define min(a,b) ((a) < (b) ? (a) : (b))
// WARNING: evaluates twice
#define max(a,b) ((a) > (b) ? (a) : (b))

// output
#define logprint(...) fprintf(stderr, __VA_ARGS__)

// Android-like logging with colors
// Error
#define ALOGE(...) logprint(ANSI_BKGRD_RED ANSI_COLOR_WHITE "[x]" ANSI_CLRST " " __VA_ARGS__)
// Warning
#define ALOGW(...) logprint(ANSI_BKGRD_YELLOW ANSI_COLOR_BLACK "[!]" ANSI_CLRST " " __VA_ARGS__)
// Information
#define ALOGI(...) logprint(ANSI_BKGRD_GREEN ANSI_COLOR_BLACK "[i]" ANSI_CLRST " " __VA_ARGS__)
// Debug
#define ALOGD(...) logprint(ANSI_BKGRD_CYAN ANSI_COLOR_BLACK "[D]" ANSI_CLRST " " __VA_ARGS__)
// Verbose
#define ALOGV(...) logprint(ANSI_BKGRD_WHITE ANSI_COLOR_BLACK "[V]" ANSI_CLRST " " __VA_ARGS__)

// https://www.guyrutenberg.com/2008/12/20/expanding-macros-into-string-constants-in-c
#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#ifdef WILDRELEASE
#define PASSSTR(x) ""
#else
#define PASSSTR(x) STR(x)
#endif

// assert (release-time)
// мягкая проверка условия. напечатает ошибку, но выполнение не прервётся
#define lassert(x) (void)((!!(x)) || syscall_print_error(PASSSTR(x), __FILE__, __LINE__, 0))
// жёсткая проверка условия. напечатает ошибку и развалит программу через abort()
#define sassert(x) (void)((!!(x)) || syscall_error(PASSSTR(x), __FILE__, __LINE__))
// жёсткая проверка условия, как и выше, ориентированная на системные вызовы
#define __syswrap(x) sassert((x) != -1)
// проверка условия и вызов пользовательской функции, если условие нарушено
#define custom_assert(x,y) (void)(!!(x) || y(PASSSTR(x), __FILE__, __LINE__))

// assert (compile-time)
//#define STATIC_ASSERT(x) { int __temp_static_assert[(x) ? -1 : 1]; }
// https://stackoverflow.com/a/3385694/1543625
#define COMPILE_TIME_ASSERT4(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
#define COMPILE_TIME_ASSERT3(X,L) COMPILE_TIME_ASSERT4(X,at_line_##L)
#define COMPILE_TIME_ASSERT2(X,L) COMPILE_TIME_ASSERT3(X,L)
#define STATIC_ASSERT(X)          COMPILE_TIME_ASSERT2(X,__LINE__)

// debug
extern bool verbose;
#define VERBOSE if(verbose)

// exported functions
// call syscall_print_error, call endwin() if there is ncurses, and call abort()
extern int syscall_error(const char *x, const char *file, int line);
// prints error string from err_no to stderr
extern int syscall_print_error(const char *x, const char *file, int line, int err_no);
