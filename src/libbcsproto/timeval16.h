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

// This is a warning-silencer for compilers where struct timeval is 
// smaller than 128 bit. PLEASE, use with care!
#define __gettimeofday_syscall(tv,tz) gettimeofday((tv),(tz))
#define gettimeofday(tv,tz) __gettimeofday_syscall((struct timeval*)(tv),(tz))

#endif
