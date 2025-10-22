#ifndef ARCH_CC_H
#define ARCH_CC_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>  // For strcasecmp and strncasecmp

// Data types
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

// Byte order - only define if not already defined by system headers
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

// Alignment
#define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

// Debugging
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__); while(1); } while(0)

// Memory allocation
#define LWIP_MALLOC_MEMPOOL_START
#define LWIP_MALLOC_MEMPOOL_END

// Random number generation
#define LWIP_RAND() ((u32_t)rand())

// String functions - provide implementations for missing functions
#ifndef stricmp
static inline int stricmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}
#endif

#ifndef strnicmp
static inline int strnicmp(const char* s1, const char* s2, size_t n) {
    return strncasecmp(s1, s2, n);
}
#endif

#define lwip_stricmp stricmp
#define lwip_strnicmp strnicmp

// Platform-specific includes
#include "pico/stdlib.h"

#endif // ARCH_CC_H
