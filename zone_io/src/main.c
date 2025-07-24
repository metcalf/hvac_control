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
volatile uint8_t port_read_flag_;
volatile uint8_t port_in_values_[3];
volatile uint8_t *ports_[] = {&VPORTA.IN, &VPORTB.IN, &VPORTC.IN};

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

void setupUSART() {
    VPORTA.DIR |= PIN1_bm; // TxD on PA1
    // Configure async 8N1 (this is the default anyway, being explicit for clarity)
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;
    USART1.BAUD = (uint16_t)((double)(F_CLK_PER * 64 / (16 * (double)UART_BAUD)) + 0.5);
    USART1.CTRLB = USART_TXEN_bm; // TX only
}

void startTx(uint8_t *port_states, uint8_t vlv12_state, uint8_t vlv34_state) {
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

    tx_buffer_[2] |= (vlv12_state << 2) | (vlv34_state << 4);

    USART1.CTRLA |= USART_DREIE_bm;
}

void setupInputPins() {
    VPORTA.DIR = 0;
    VPORTB.DIR = 0;
    VPORTC.DIR = 0;

    register8_t *pins[] = {
        &PORTA.PIN2CTRL, &PORTA.PIN3CTRL, &PORTA.PIN4CTRL, &PORTA.PIN5CTRL,
        &PORTA.PIN6CTRL, &PORTA.PIN7CTRL,

        &PORTB.PIN0CTRL, &PORTB.PIN1CTRL, &PORTB.PIN2CTRL, &PORTB.PIN3CTRL,
        &PORTB.PIN4CTRL, &PORTB.PIN5CTRL,

        &PORTC.PIN0CTRL, &PORTC.PIN1CTRL, &PORTC.PIN2CTRL, &PORTC.PIN3CTRL,
    };

    for (int i = 0; i < (sizeof(pins) / sizeof(pins[0])); i++) {
        // Enable pullups on all input pins and invert inputs since NPN optos pull down
        *pins[i] = (PORT_PULLUPEN_bm | PORT_INVEN_bm);
    }
}

void setupPinCheckTimer() {
    // Sample at 1200hz (10x rectified mains frequency)
    TCA0.SINGLE.PER = (F_CLK_PER + 600) / 1200;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_16X_gc; // Divide main clock by 16 = 1.25mhz

    setupInputPins();
    setupUSART();
    setupPinCheckTimer();

    sei(); // Enable interrupts

    while (1) {
        wdt_reset();

        uint8_t port_counts[3][8] = {};

        // Poll 100 times at 1200hz. Gives us 10 full cycles of data and takes ~83ms
        // Our read strategy here is designed to filter out external noise at the optoisolator inputs.
        // The Taco valves in particular throw off a lot of noise and cause pins to flicker.
        port_read_flag_ = 0;
        TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; // Clear interrupt flag
        TCA0.SINGLE.CNT = 0;                       // Start at zero
        TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; // Start the timer

        for (int i = 0; i < 100; i++) {
            while (1) {
                uint8_t read;
                ATOMIC_BLOCK(ATOMIC_FORCEON) { read = port_read_flag_; }
                if (read) {
                    break;
                }
            }

            for (uint8_t port_idx = 0; port_idx < 3; port_idx++) {
                for (uint8_t pin = 0; pin < 8; pin++) {
                    if (port_in_values_[port_idx] & (1 << pin)) {
                        port_counts[port_idx][pin]++;
                    }
                }
            }
            ATOMIC_BLOCK(ATOMIC_FORCEON) { port_read_flag_ = 0; }
        }

        TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm; // Stop the timer

        uint8_t port_states[3] = {0};

        for (uint8_t port_idx = 0; port_idx < 3; port_idx++) {
            for (uint8_t pin = 0; pin < 8; pin++) {
                // The optoisolators are pretty sensitive so it should read high
                // most of the time if the pin is high. The internal pull-ups is very weak (~35kohm)
                // so a tiny amount of current (<0.1mA) should pull the pin high. This corresponds to
                // something like 0.2mA at the opto input which requires 5-6V.
                if (port_counts[port_idx][pin] > 60) {
                    port_states[port_idx] |= (1 << pin);
                }
            }
        }

        uint8_t vlv_states[2] = {0};
        for (int i = 0; i < 2; i++) {
            // PC2 and PC3 are used for VLV1/2 and VLV3/4 respectively
            uint8_t count = port_counts[2][i + 2];

            if (count > 60) {
                vlv_states[i] = 2; // Both channels on
            } else if (count > 30) {
                vlv_states[i] = 1; // One channel on
            } else {
                vlv_states[i] = 0; // Off
            }
        }

        startTx(port_states, vlv_states[0], vlv_states[1]);

        // Delay while sending since we don't need the data very frequently
        wdt_reset();
        _delay_ms(SEND_INTERVAL_MS);
    }
}

ISR(TCA0_OVF_vect) {
    if (port_read_flag_ == 0) {
        for (int i = 0; i < 3; i++) {
            port_in_values_[i] = *ports_[i];
        }
        port_read_flag_ = 1;
    }

    TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; // Clear interrupt flag
}

ISR(USART1_DRE_vect) {
    USART1_TXDATAL = tx_buffer_[tx_pos_];
    tx_pos_ = (tx_pos_ + 1) % sizeof(tx_buffer_);

    if (tx_pos_ == 0) {
        // Write complete, disable interrupt
        USART1.CTRLA &= ~USART_DREIE_bm;
    }
}
