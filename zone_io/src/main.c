#include <stdint.h>
#include <string.h>

#include "avr/interrupt.h"
#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"
#include "util/delay.h"

#define VLV_SW_CLK F_CLK_PER / 2
#define UART_BAUD 9600
#define SEND_INTERVAL_MS 450

volatile uint16_t vlv12_sw_period_, vlv34_sw_period_;

volatile uint8_t tx_pos_;
char tx_buffer_[3];

const uint8_t tx_map_[3][6] = {
    {
        // PORTA
        PIN2_bm, // FC1_V
        PIN3_bm, // FC1_OBpwm
        PIN4_bm, // FC2_V
        PIN5_bm, // FC2_OB
        PIN6_bm, // FC3_V
        PIN7_bm, // FC3_OB
    },
    {
        // PORTB
        PIN5_bm, // FC4_V
        PIN4_bm, // FC4_OB
        PIN1_bm, // TS1_W
        PIN2_bm, // TS1_Y
        PIN3_bm, // TS2_W
        PIN0_bm, // TS2_Y
    },
    {
        // PORTC
        PIN0_bm, // TS3_W
        PIN1_bm, // TS4_W
    },
};

uint8_t getVlvState(uint16_t *g_period) {
    uint16_t period;
    ATOMIC_BLOCK(ATOMIC_FORCEON) { period = *g_period; }

    if (period == 0) {
        return 0;
    }

    // Integer rounding division to convert period to frequency
    uint32_t vlv_sw_freq = (VLV_SW_CLK + period / 2) / period;

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

void startTx(uint8_t *port_states) {
    if (USART1.CTRLA & USART_DREIE_bm) {
        return; // We're still writing, skip this Tx
    }

    for (int i = 0; i < 3; i++) {
        int lim = i < 2 ? 6 : 2;
        tx_buffer_[i] = i << 6; // Set payload ID in last 2 bits
        for (int j = 0; j < lim; j++) {
            if (port_states[i] & tx_map_[i][j]) {
                tx_buffer_[i] |= (0x01 << j);
            }
        }
    }

    tx_buffer_[2] |= (getVlvState(&vlv12_sw_period_) << 2) | (getVlvState(&vlv34_sw_period_) << 4);

    USART1.CTRLA |= USART_DREIE_bm;
}

void setupInputPins() {
    VPORTA.DIR = 0;
    VPORTB.DIR = 0;
    VPORTC.DIR = 0;

    register8_t *pins[] = {
        &PORTA.PIN2CTRL, &PORTA.PIN3CTRL, &PORTA.PIN4CTRL, &PORTA.PIN5CTRL, &PORTA.PIN6CTRL,
        &PORTA.PIN7CTRL, &PORTB.PIN1CTRL, &PORTB.PIN2CTRL, &PORTB.PIN3CTRL, &PORTB.PIN5CTRL,
        &PORTB.PIN4CTRL, &PORTB.PIN0CTRL, &PORTC.PIN0CTRL, &PORTC.PIN1CTRL, &PORTC.PIN2CTRL,
    };

    for (int i = 0; i < (sizeof(pins) / sizeof(pins[0])); i++) {
        // Enable pullups on all input pins and invert inputs since NPN optos pull down
        *pins[i] = (PORT_PULLUPEN_bm | PORT_INVEN_bm);
    }
}

void setupTCBn(TCB_t tcb) {
    tcb.CTRLB = TCB_CNTMODE_FRQ_gc;           // Frequency count mode
    tcb.EVCTRL = TCB_CAPTEI_bm | TCB_EDGE_bm; // Measure frequency between falling edge events
    tcb.INTCTRL = TCB_CAPT_bm;
    tcb.CTRLA =
        TCB_ENABLE_bm | TCB_CLKSEL_DIV2_gc; // Configure tach frequency measurement @ ~156khz
}

void setupVlvTimer() {
    // Configure TCB0 on PC2 for frequency measurement for VLV1/2_SW_IO
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm;
    EVSYS.CHANNEL2 = EVSYS_CHANNEL2_PORTC_PIN2_gc; // Route pin PC2
    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL2_gc;   // to TCB0
    setupTCBn(TCB0);

    // Configure TCB1 on PC3 for frequency measurement for VLV3/4_SW_IO
    PORTC.PIN3CTRL = PORT_PULLUPEN_bm;
    EVSYS.CHANNEL3 = EVSYS_CHANNEL2_PORTC_PIN3_gc; // Route pin PC3
    EVSYS.USERTCB1CAPT = EVSYS_USER_CHANNEL3_gc;   // to TCB1
    setupTCBn(TCB1);
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    setupInputPins();
    setupUSART();
    setupVlvTimer();

    sei(); // Enable interrupts

    while (1) {
        wdt_reset();

        uint8_t port_states[3] = {0, 0, 0};

        // Poll pin states every 833us for 50ms. This gives us a sample rate
        // of 1200hz on a 120hz signal for 6 cycles
        for (int i = 0; i < 60; i++) {
            port_states[0] |= VPORTA.IN;
            port_states[1] |= VPORTB.IN;
            port_states[2] |= VPORTC.IN;

            _delay_us(833);
        }

        startTx(port_states);

        // Delay while sending since we don't need the data very frequently
        wdt_reset();
        _delay_ms(SEND_INTERVAL_MS);
    }
}

uint16_t handleVlvInt(TCB_t tcb) {
    if (tcb.INTFLAGS & TCB_OVF_bm) {
        // If we overflowed, clear both flags and set an invalid period
        tcb.INTFLAGS |= TCB_OVF_bm;
        tcb.INTFLAGS |= TCB_CAPT_bm;
        return 0;
    } else {
        return tcb.CCMP; // reading CCMP clears interrupt flag
    }
}

ISR(TCB0_INT_vect) { vlv12_sw_period_ = handleVlvInt(TCB0); }
ISR(TCB1_INT_vect) { vlv34_sw_period_ = handleVlvInt(TCB1); }

ISR(USART1_DRE_vect) {
    USART1_TXDATAL = tx_buffer_[tx_pos_];
    tx_pos_ = (tx_pos_ + 1) % sizeof(tx_buffer_);

    if (tx_pos_ == 0) {
        // Write complete, disable interrupt
        USART1.CTRLA &= ~USART_DREIE_bm;
    }
}
