/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * LwIP Architecture - Compiler/CPU definitions for STM32H5
 */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Define timeval structure for embedded (no sys/time.h) */
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval
{
    long tv_sec;  /* seconds */
    long tv_usec; /* microseconds */
};
#endif

/* Define basic types for LwIP */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

/* Define byte order - ARM Cortex-M is little endian */
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* Define (sn)printf formatters for these LwIP types
 * On ARM Cortex-M, uint32_t is 'unsigned long', so use "lu" format */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"
#define SZT_F "zu"

/* Compiler hints for packing structures */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT   __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Platform specific diagnostic output */
#define LWIP_PLATFORM_DIAG(x) \
    do                        \
    {                         \
        printf x;             \
    } while (0)
#define LWIP_PLATFORM_ASSERT(x)                                           \
    do                                                                    \
    {                                                                     \
        printf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, \
               __FILE__);                                                 \
        while (1);                                                        \
    } while (0)

/* Error codes - use errno values */
#define LWIP_ERRNO_INCLUDE    <errno.h>
#define LWIP_ERRNO_STDINCLUDE 1

/* Random number generation */
#define LWIP_RAND() ((u32_t)rand())

/* Compiler hints */
#ifndef LWIP_NO_STDINT_H
#define LWIP_NO_STDINT_H 0
#endif

#ifndef LWIP_NO_INTTYPES_H
#define LWIP_NO_INTTYPES_H 0
#endif

#ifndef LWIP_NO_LIMITS_H
#define LWIP_NO_LIMITS_H 0
#endif

#endif /* LWIP_ARCH_CC_H */
