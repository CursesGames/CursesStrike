// It's better without
//#pragma once

// ReSharper disable once CppMissingIncludeGuard
#ifdef _MSC_VER
// try include_next, do not work now!
//#include_next <stdbool.h>
//#include_next <alloca.h>

// fix macro
#undef __cplusplus
// mute gcc-specific macro
#define __builtin_alloca NULL // in real MSVC: _alloca(size)
#define __builtin_popcount(val) 0

#define be16toh(x) (uint16_t)(x)
#define be32toh(x) (uint32_t)(x)
#define be64toh(x) (uint64_t)(x)

#define htobe16(x) (uint16_t)(x)
#define htobe32(x) (uint32_t)(x)
#define htobe64(x) (uint64_t)(x)
#endif

#define assert #error "Avoid using assert"
