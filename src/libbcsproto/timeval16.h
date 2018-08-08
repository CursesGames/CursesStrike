#pragma once

#ifdef TIMEVAL16

struct timeval_x {
	time_t tv_sec;
	suseconds_t tv_usec;
	char _pad[16 - sizeof(struct timeval)];
};

#define timeval timeval_x

#endif
