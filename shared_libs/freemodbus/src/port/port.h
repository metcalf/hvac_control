/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

#ifndef _PORT_H
#define _PORT_H

/* ----------------------- Platform includes --------------------------------*/

#include <avr/interrupt.h>
#include <avr/io.h>

/* ----------------------- Defines ------------------------------------------*/
#define INLINE inline
#define PR_BEGIN_EXTERN_C extern "C" {
#define PR_END_EXTERN_C }

#define ENTER_CRITICAL_SECTION() cli()
#define EXIT_CRITICAL_SECTION() sei()

#define assert(x)

// The Macros below handle the endianness while transfer N byte data into buffer
#define _XFER_4_RD(dst, src)                                                                       \
    {                                                                                              \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 1);                                               \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 0);                                               \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 3);                                               \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 2);                                               \
        (src) += 4;                                                                                \
    }

#define _XFER_2_RD(dst, src)                                                                       \
    {                                                                                              \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 1);                                               \
        *(uint8_t *)(dst)++ = *(uint8_t *)(src + 0);                                               \
        (src) += 2;                                                                                \
    }

#define _XFER_4_WR(dst, src)                                                                       \
    {                                                                                              \
        *(uint8_t *)(dst + 1) = *(uint8_t *)(src)++;                                               \
        *(uint8_t *)(dst + 0) = *(uint8_t *)(src)++;                                               \
        *(uint8_t *)(dst + 3) = *(uint8_t *)(src)++;                                               \
        *(uint8_t *)(dst + 2) = *(uint8_t *)(src)++;                                               \
    }

#define _XFER_2_WR(dst, src)                                                                       \
    {                                                                                              \
        *(uint8_t *)(dst + 1) = *(uint8_t *)(src)++;                                               \
        *(uint8_t *)(dst + 0) = *(uint8_t *)(src)++;                                               \
    }

typedef char BOOL;

typedef unsigned char UCHAR;
typedef char CHAR;

typedef unsigned short USHORT;
typedef short SHORT;

typedef unsigned long ULONG;
typedef long LONG;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include "./mbconfig.h"

#endif
