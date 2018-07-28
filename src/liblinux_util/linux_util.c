// Note: this include is a beta feature for design- and compile-time
#include "mscfix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "linux_util.h"

bool verbose = false;

int syscall_print_error(const char *x, const char *file, const int line, const int err_no) {
	if (err_no != 0) {
		char *errstr = strerror(err_no);
		logprint("[x] syscall err: '%s'\n\tat %s:%d (%d = %s)\n", x, file, line, err_no, errstr);
	}
	else {
		logprint("[x] runtime err: '%s'\n\tat %s:%d\n", x, file, line);
	}
	return EXIT_SUCCESS;
}

int syscall_error(const char *x, const char *file, const int line) {
	// Turn off curses mode if we have it
	// `#define CURSES 1' is from <ncurses.h>
#ifdef CURSES
	endwin();
#endif

	// Get errno and errstr for last syscall
	int err_no = errno;
	syscall_print_error(x, file, line, err_no);
	abort();
	return EXIT_FAILURE;
}
