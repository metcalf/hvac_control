#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define UART_BAUD 9600

volatile uint16_t vlv_sw_period_;

void setupUSART() {
    VPORTA.DIR |= PIN1_bm;        // TxD on PA1
    USART1.CTRLB = USART_TXEN_bm; // TX only
    USART1.BAUD = (uint16_t)((float)(F_CLK_PER * 64 / (16 * (float)UART_BAUD)) + 0.5);
    // Configure async 8N1 (this is the default anyway, being explicit for clarity)
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;
}

int main(void) {
    wdt_enable(WDTO_1S);

    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    setupUSART();

    // TODO: Enable pull-ups on ~everything

    // Configure TCB0 on WO0/PC0 for frequency measurement for VLV_SW_IO
    PORTMUX.TCBROUTEA = PORTMUX_TCB0_ALT1_gc;
    TCB0.CTRLB = TCB_CNTMODE_FRQ_gc; // Frequency count mode
    //TCB0.EVCTRL = TCB_CAPTEI_bm; // I had this set originally but think it isn't necessary?
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA =
        (TCB_ENABLE_bm | TCB_CLKSEL_DIV2_gc); // Configure tach frequency measurement @ ~156khz

    // TODO:
    // * Configure tick counting
    // * Setup loop for polling state
    // * Setup UART for periodically sending data

    while (1) {
        wdt_reset();
    }
}

ISR(TCB0_INT_vect) {
    if (TCB0.INTFLAGS & TCB_OVF_bm) {
        // If we overflowed, clear the overflow bit and set an invalid period
        TCB0.INTFLAGS &= ~TCB_OVF_bm;
        vlv_sw_period_ = 0;
    } else {
        vlv_sw_period_ = TCB0.CCMP; // reading CCMP clears interrupt flag
    }
}
