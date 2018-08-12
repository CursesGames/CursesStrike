#pragma once

#include <time.h>
// this may be useful for MIPS
// ReSharper disable once CppUnusedIncludeDirective
#include <sys/time.h>

#if __WORDSIZE == 64
typedef struct timeval timeval128_t;
#else
typedef struct {
    time_t tv_sec;
    suseconds_t tv_usec;
    char _pad[16 - sizeof(struct timeval)];
} timeval128_t;
#endif
