#include <stdint.h>
#include <stdlib.h>

#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"

#include "modbus_client.h"
#include "usart_debug.h"

#define SLAVE_ID 0x23
#define MB_BAUD 9600
#define OUTPUT_PIN_NUM PIN5_bm
#define INPUT_PIN_NUM PIN0_bm
#define TICK_PERIOD_MS 100
#define INPUT_READ_TIME_MS (1000 * 60) // Clear input state if no ON seen in 60 seconds
#define INPUT_READ_TIME_TICKS (INPUT_READ_TIME_MS / TICK_PERIOD_MS) // Must be less than 2^16

volatile uint16_t tick_;
uint16_t input_last_on_;

bool input_state_;

uint16_t getTick() {
    ATOMIC_BLOCK(ATOMIC_FORCEON) { return tick_; }
}

void setOutput(bool state) {
    if (state) {
        VPORTB.OUT |= OUTPUT_PIN_NUM;
    } else {
        VPORTB.OUT &= ~OUTPUT_PIN_NUM;
    }
}

bool readInput() {
    bool val = input_state_;
    input_state_ = false;
    return val;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    // NB: Update F_CLK_PER in platformio.ini if this changes
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_32X_gc; // Divide main clock by 32 = 625khz

    USART_DEBUG_INIT();
    USART_DEBUG_SEND('a');

    // Use TCB0 as tick counter
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CCMP = F_CLK_PER / 10; // Run at 10hz for tick counting
    TCB0.CTRLA = TCB_ENABLE_bm; // TODO: configure this frequency and set tick rate

    // Enable pullup resistor on input PC0 and invert value
    PORTC.PIN0CTRL |= (PORT_PULLUPEN_bm | PORT_INVEN_bm);
    // Output on PB5
    VPORTB.DIR |= PIN5_bm;
    PORTB.PIN5CTRL |= PORT_INVEN_bm;

    modbus_client_init(SLAVE_ID, MB_BAUD, readInput, setOutput);

    sei();

    USART_DEBUG_SEND('s');
    while (1) {
        wdt_reset();
        modbus_poll();

        uint16_t now = getTick();

        if (now % (1000 / TICK_PERIOD_MS) == 0) {
            //USART_DEBUG_SEND('.');
        }

        if (VPORTC.IN & INPUT_PIN_NUM) {
            // Any ON value read indicates an ON state
            if (!input_state_) {
                USART_DEBUG_SEND('+');
            }
            input_state_ = 1;
            input_last_on_ = now;
        } else if ((now - input_last_on_) > INPUT_READ_TIME_TICKS) {
            if (input_state_) {
                USART_DEBUG_SEND('-');
            }
            // If we haven't read an ON value in INPUT_READ_TIME_TICKS, it's off
            input_state_ = 0;
        }
    }
}

ISR(TCB0_INT_vect) {
    TCB0.INTFLAGS |= TCB_CAPT_bm;
    tick_++;
}
