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

// output
#define logprint(...) fprintf(stderr, __VA_ARGS__)

// Android-like logging
#define ALOGE(...) logprint(ANSI_BKGRD_RED ANSI_COLOR_WHITE "[x]" ANSI_CLRST " " __VA_ARGS__)
#define ALOGW(...) logprint(ANSI_BKGRD_YELLOW ANSI_COLOR_BLACK "[!]" ANSI_CLRST " " __VA_ARGS__)
#define ALOGI(...) logprint(ANSI_BKGRD_GREEN ANSI_COLOR_BLACK "[i]" ANSI_CLRST " " __VA_ARGS__)
#define ALOGD(...) logprint(ANSI_BKGRD_CYAN ANSI_COLOR_BLACK "[D]" ANSI_CLRST " " __VA_ARGS__)
#define ALOGV(...) logprint(ANSI_BKGRD_WHITE ANSI_COLOR_BLACK "[V]" ANSI_CLRST " " __VA_ARGS__)

// assert (release-time)
#define lassert(x) (void)((!!(x)) || syscall_print_error(#x, __FILE__, __LINE__, 0))
#define sysassert(x) (void)((!!(x)) || syscall_error(#x, __FILE__, __LINE__))
#define __syscall(x) sysassert((x) != -1)

// debug
extern bool verbose;
#define VERBOSE if(verbose)

// exported functions
extern int syscall_error(const char *x, const char *file, int line);
extern int syscall_print_error(const char *x, const char *file, int line, int err_no);
