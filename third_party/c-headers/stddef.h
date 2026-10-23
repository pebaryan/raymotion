#ifndef RAYMOTION_STDDEF_SHIM
#define RAYMOTION_STDDEF_SHIM
/* Minimal subset of <stddef.h>: this machine's slim VS install lacks it. */
typedef long long          ptrdiff_t;
typedef unsigned long long size_t;
typedef long long          intptr_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
#endif
