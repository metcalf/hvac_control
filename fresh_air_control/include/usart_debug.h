#pragma once

#include "avr/io.h"

#ifdef USART_DEBUG
#define USART_DEBUG_INIT() usart_debug_init()
#define USART_DEBUG_SEND(c) usart_debug_send(c)
#else
#define USART_DEBUG_INIT()
#define USART_DEBUG_SEND(c)
#endif

void usart_debug_init() {
    PORTMUX.USARTROUTEA |= PORTMUX_USART0_ALT1_gc;
    VPORTA.DIR |= PIN1_bm; // TxD on PA1
    USART0.BAUD = (uint16_t)((float)(F_CLK_PER * 64 / (16 * (float)9600)) + 0.5);
    USART0.CTRLB = USART_TXEN_bm; // TX only
}

void usart_debug_send(char byte) {
    while (!(USART0.STATUS & USART_DREIF_bm)) {
        ;
    }
    USART0_TXDATAL = byte;
}
