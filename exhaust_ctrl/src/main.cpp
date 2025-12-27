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
#define TICK_PERIOD_MS 5
#define INPUT_DEBOUNCE_MS 20 // Require input high for 20ms before counting as on
#define INPUT_DEBOUNCE_TICKS (INPUT_DEBOUNCE_MS / TICK_PERIOD_MS) // 4 ticks
#define INPUT_READ_TIME_MS (1000 * 60) // Clear input state if no ON seen in 60 seconds
#define INPUT_READ_TIME_TICKS (INPUT_READ_TIME_MS / TICK_PERIOD_MS) // Must be less than 2^16

volatile uint16_t tick_;
uint16_t input_state_last_on_tick_;
uint16_t input_first_on_tick_;
bool input_is_on_;

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
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_16X_gc; // Divide main clock by 16 = 1.25mhz

    USART_DEBUG_INIT();
    USART_DEBUG_SEND('a');

    // Use TCB0 as tick counter
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CCMP = F_CLK_PER / 200; // Run at 200hz for tick counting
    TCB0.CTRLA = TCB_ENABLE_bm;  // TODO: configure this frequency and set tick rate

    // Enable pullup resistor on input PC0 and invert value
    PORTC.PIN0CTRL |= (PORT_PULLUPEN_bm | PORT_INVEN_bm);
    // Output on PB5
    VPORTB.DIR |= PIN5_bm;

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
            // Track when input first went high
            if (!input_is_on_) {
                input_first_on_tick_ = now;
                input_is_on_ = true;
            }
            // Only set state to ON after input has been high for required ticks
            if ((now - input_first_on_tick_) >= INPUT_DEBOUNCE_TICKS) {
                if (!input_state_) {
                    USART_DEBUG_SEND('+');
                }
                input_state_ = 1;
                input_state_last_on_tick_ = now;
            }
        } else {
            // Input is low, reset tracking
            input_is_on_ = false;
            if ((now - input_state_last_on_tick_) > INPUT_READ_TIME_TICKS) {
                if (input_state_) {
                    USART_DEBUG_SEND('-');
                }
                // If we haven't read an ON value in INPUT_READ_TIME_TICKS, it's off
                input_state_ = 0;
            }
        }
    }
}

ISR(TCB0_INT_vect) {
    TCB0.INTFLAGS |= TCB_CAPT_bm;
    tick_++;
}
