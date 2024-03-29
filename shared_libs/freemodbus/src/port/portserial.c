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

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include <freemodbus.h>

/* ----------------------- Start implementation -----------------------------*/
void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable) {
    /* If xRXEnable enable serial receive interrupts. If xTxENable enable
     * transmitter empty interrupts.
     */

    if (xRxEnable) {
        USART1.CTRLA |= USART_RXCIE_bm;
    } else {
        USART1.CTRLA &= ~USART_RXCIE_bm;
    }

    if (xTxEnable) {
        USART1.CTRLA |= USART_DREIE_bm;
    } else {
        USART1.CTRLA &= ~USART_DREIE_bm;
    }
}

BOOL xMBPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity) {
    assert(eParity == MB_PAR_NONE); // Don't support parity
    assert(ucPORT == 0x00);         // Don't support selecting port

    PORTMUX.USARTROUTEA |= PORTMUX_USART1_ALT1_gc;
    VPORTC.DIR &= ~PIN1_bm;            // RxD on PC1
    VPORTC.DIR |= (PIN2_bm | PIN3_bm); // TxD on PC2, XDIR on PC3

    USART1.CTRLA = USART_RS485_bm;
    USART1.BAUD = (uint16_t)((float)(F_CLK_PER * 64 / (16 * (float)ulBaudRate)) + 0.5);
    // Configure async 8N1 (this is the default anyway, being explicit for clarity)
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;
    USART1.CTRLB = USART_TXEN_bm | USART_RXEN_bm;

    return TRUE;
}

BOOL xMBPortSerialPutByte(CHAR ucByte) {
    /* Put a byte in the UARTs transmit buffer. This function is called
     * by the protocol stack if pxMBFrameCBTransmitterEmpty( ) has been
     * called. */

    USART1_TXDATAL = ucByte;
    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *pucByte) {
    /* Return the byte in the UARTs receive buffer. This function is called
     * by the protocol stack after pxMBFrameCBByteReceived( ) has been called.
     */
    *pucByte = USART1_RXDATAL;

    return TRUE;
}

/* Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call
 * xMBPortSerialPutByte( ) to send the character.
 */
ISR(USART1_DRE_vect) { pxMBFrameCBTransmitterEmpty(); }

/* Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
ISR(USART1_RXC_vect) {
    // Interrupt flag is cleared when data is read from the rx buffer
    pxMBFrameCBByteReceived();
}
