/* Minimal arch/cc.h for building lwIP with mingw-w64 as a POC. */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Basic type definitions expected by lwIP */
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint8_t u8_t;
typedef int8_t s8_t;

/* mem pointer type used by lwIP's memory macros */
typedef uintptr_t mem_ptr_t;

/* Alignment expected by many lwIP implementations on x86/x86_64 */
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT 4
#endif

/* Byte order: little endian for x86_64 */
#define LITTLE_ENDIAN 1234
#define BYTE_ORDER LITTLE_ENDIAN

/* Packing macros for GCC/clang */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Platform-specific macros used in lwIP */
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { fprintf(stderr, "LWIP ASSERT: %s\n", x); abort(); } while(0)

#define LWIP_PLATFORM_BYTESWAP 0

#endif /* LWIP_ARCH_CC_H */
