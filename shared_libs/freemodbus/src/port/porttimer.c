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

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include <freemodbus.h>

/* ----------------------- Defines ------------------------------------------*/
#define MB_TIMER_PRESCALER (2UL)
#define MB_TIMER_TICKS (F_CLK_PER / MB_TIMER_PRESCALER)
#define MB_50US_TICKS (20000UL)

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBPortTimersInit(USHORT usTim1Timerout50us) {
    TCB1.CTRLA = TCB_CLKSEL_DIV2_gc;
    TCB1.CTRLB = 0;
    TCB1.INTCTRL = TCB_CAPT_bm;
    TCB1.CNT = 0;
    TCB1.CCMP = (MB_TIMER_TICKS * usTim1Timerout50us) / (MB_50US_TICKS);

    return TRUE;
}

inline void vMBPortTimersEnable() { TCB1.CTRLA |= TCB_ENABLE_bm; }

inline void vMBPortTimersDisable() { TCB1.CTRLA &= ~(TCB_ENABLE_bm); }

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
ISR(TCB1_INT_vect) {
    if (TCB0.INTFLAGS & TCB_CAPT_bm) {
        (void)pxMBPortCBTimerExpired();
    }
}
