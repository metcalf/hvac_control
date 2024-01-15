#include <stdint.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define VLV_SW_CLK F_CLK_PER / 2
#define UART_BAUD 9600
#define SEND_INTERVAL_MS 450

volatile uint16_t vlv_sw_period_;

uint8_t port_states_[3];
volatile uint8_t tx_pos_;
char tx_buffer_[3];

uint8_t getVlvState() {
    if (vlv_sw_period_ == 0) {
        return 0;
    }

    uint8_t vlv_sw_freq = VLV_SW_CLK / vlv_sw_period_;
    // NB: Rectified AC is double mains frequency (120hz)
    if (vlv_sw_freq < 30) {
        return 0; // Assume off
    } else if (vlv_sw_freq < 90) {
        return 1; // Assume one channel on (~60hz)
    } else {
        return 2; // Assume both channels on (~120hz)
    }
}

void setupUSART() {
    VPORTA.DIR |= PIN1_bm; // TxD on PA1
    // Configure async 8N1 (this is the default anyway, being explicit for clarity)
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;
    USART1.BAUD = (uint16_t)((float)(F_CLK_PER * 64 / (16 * (float)UART_BAUD)) + 0.5);
    USART1.CTRLB = USART_TXEN_bm; // TX only
}

void startTx() {
    if (USART1.CTRLA & USART_DREIE_bm) {
        return; // We're still writing, skip this Tx
    }

    tx_buffer_[0] = (VPORTA.IN & PIN2_bm && (1 << 0)) | // FC1_V
                    (VPORTA.IN & PIN3_bm && (1 << 1)) | // FC1_OB
                    (VPORTA.IN & PIN4_bm && (1 << 2)) | // FC2_V
                    (VPORTA.IN & PIN5_bm && (1 << 3)) | // FC2_OB
                    (VPORTA.IN & PIN6_bm && (1 << 4)) | // FC3_V
                    (VPORTA.IN & PIN7_bm && (1 << 5));  // FC3_OB

    tx_buffer_[1] = (VPORTB.IN & PIN5_bm && (1 << 0)) | // FC4_V
                    (VPORTB.IN & PIN4_bm && (1 << 1)) | // FC4_OB
                    (VPORTB.IN & PIN1_bm && (1 << 2)) | // TS1_W
                    (VPORTB.IN & PIN2_bm && (1 << 3)) | // TS1_Y
                    (VPORTB.IN & PIN3_bm && (1 << 4)) | // TS2_W
                    (VPORTB.IN & PIN0_bm && (1 << 5)) | // TS2_Y
                    // Payload ID 1
                    (1 << 6);

    tx_buffer_[2] = (VPORTC.IN & PIN1_bm && (1 << 0)) | // TS3_W
                    (VPORTC.IN & PIN2_bm && (1 << 1)) | // TS3_Y
                    (VPORTC.IN & PIN3_bm && (1 << 2)) | // TS4_W
                    (getVlvState() << 4) |
                    // Payload ID 2
                    (1 << 7);

    USART1.CTRLA |= USART_DREIE_bm;
}

void setupPins() {
    PORTA.PIN2CTRL |= PORT_PULLUPEN_bm; // FC1_V
    PORTA.PIN3CTRL |= PORT_PULLUPEN_bm; // FC1_OB
    PORTA.PIN4CTRL |= PORT_PULLUPEN_bm; // FC2_V
    PORTA.PIN5CTRL |= PORT_PULLUPEN_bm; // FC2_OB
    PORTA.PIN6CTRL |= PORT_PULLUPEN_bm; // FC3_V
    PORTA.PIN7CTRL |= PORT_PULLUPEN_bm; // FC3_OB
    PORTB.PIN1CTRL |= PORT_PULLUPEN_bm; // TS1_W
    PORTB.PIN2CTRL |= PORT_PULLUPEN_bm; // TS1_Y
    PORTB.PIN3CTRL |= PORT_PULLUPEN_bm; // TS2_W
    PORTB.PIN5CTRL |= PORT_PULLUPEN_bm; // FC4_V
    PORTB.PIN4CTRL |= PORT_PULLUPEN_bm; // FC4_OB
    PORTB.PIN0CTRL |= PORT_PULLUPEN_bm; // TS2_Y
    PORTC.PIN0CTRL |= PORT_PULLUPEN_bm; // TS3_W
    PORTC.PIN1CTRL |= PORT_PULLUPEN_bm; // TS3_W
    PORTC.PIN2CTRL |= PORT_PULLUPEN_bm; // TS3_Y
}

void setupVlvTimer() {
    // Configure TCB0 on WO0/PC0 for frequency measurement for VLV_SW_IO
    PORTMUX.TCBROUTEA = PORTMUX_TCB0_ALT1_gc;
    TCB0.CTRLB = TCB_CNTMODE_FRQ_gc; // Frequency count mode
    //TCB0.EVCTRL = TCB_CAPTEI_bm; // I had this set originally but think it isn't necessary?
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA =
        (TCB_ENABLE_bm | TCB_CLKSEL_DIV2_gc); // Configure tach frequency measurement @ ~156khz
}

int main(void) {
    //wdt_enable(WDTO_1S);

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    setupUSART();
    setupPins();
    setupVlvTimer();

    sei(); // Enable interrupts

    while (1) {
        wdt_reset();

        // Poll pin states every 833us for 50ms. This gives us a sample rate
        // of 1200hz on a 120hz signal for 6 cycles
        memset(port_states_, 0, sizeof(port_states_));
        for (int i = 0; i < 60; i++) {
            // Exclude PA0(UDPI) and PA1(UART)
            port_states_[0] |= VPORTA.IN & ~(PIN0_bm | PIN1_bm);
            port_states_[1] |= VPORTB.IN;
            // Exclude PC0 (VLV_SW)
            port_states_[2] |= VPORTC.IN & ~(PIN0_bm);

            _delay_us(833);
        }

        startTx();

        // Delay while sending since we don't need the data very frequently
        wdt_reset();
        _delay_ms(SEND_INTERVAL_MS);
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

ISR(USART1_DRE_vect) {
    USART1_TXDATAL = tx_buffer_[tx_pos_];
    tx_pos_ = (tx_pos_ + 1) % sizeof(tx_buffer_);

    if (tx_pos_ == 0) {
        // Write complete, disable interrupt
        USART1.CTRLA &= ~USART_DREIE_bm;
    }
}
