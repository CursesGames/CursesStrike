#pragma once

#ifdef _MSC_VER
// try include_next, do not work now!
//#include_next <stdbool.h>
//#include_next <alloca.h>

// fix macro
#undef __cplusplus
// mute gcc-specific macro
#define __builtin_alloca NULL // in real MSVC: _alloca(size)
#define __builtin_popcount(val) 0
#endif

#define assert #error "Avoid using assert"
