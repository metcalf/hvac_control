#include <stdint.h>
#include <stdlib.h>

#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"

#include "modbus_client.h"
#include "usart_debug.h"

#define MB_BAUD 9600
#define INPUT_PIN_NUM PIN0_bm
#define TICK_PERIOD_MS 1
#define INPUT_READ_TIME_TICKS                                                                      \
    (50 / TICK_PERIOD_MS) // Attempt to detect AC for 50ms (3 cycles @ 60hz)
#define IR_FREQ 38000
#define TCA_CLK_DIV 1
#define IR_PERIOD (F_CLK_PER / (2.0 * TCA_CLK_DIV * IR_FREQ) + 0.5)

volatile uint16_t tick_;
uint16_t iso_input_last_on_;
uint16_t iso_input_state_;

uint16_t getTick() {
    ATOMIC_BLOCK(ATOMIC_FORCEON) { return tick_; }
}

uint8_t setFancoil(bool cool_mode, uint8_t speed) {
    USART_DEBUG_SEND(cool_mode ? 'c' : 'h');
    USART_DEBUG_SEND(speed + 0x30);

    TCA0.SINGLE.CMP2BUF = IR_PERIOD * speed / 3;
    return 0;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    // TODO: Update F_CLK_PER in platformio.ini if this changes
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_4X_gc; // Run clock at 5mhz (max at 3.3V)

    USART_DEBUG_INIT();
    USART_DEBUG_SEND('a');

    // Use TCB0 as tick counter
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CCMP = F_CLK_PER / 1000; // Run at ~1khz for tick counting
    TCB0.CTRLA = TCB_ENABLE_bm;   // TODO: configure this frequency and set tick rate

    // TCA0 for IR transmission on WO2
    VPORTB.DIR |= PIN5_bm;                      // Output
    PORTMUX.TCAROUTEA |= PORTMUX_TCA02_ALT1_gc; // Use alternate pin for WO2 (PB5 / pin 9)
    TCA0.SINGLE.CTRLB =
        TCA_SINGLE_CMP2EN_bm |
        TCA_SINGLE_WGMODE_DSBOTTOM_gc; // Dual-slope PWM (TODO whether this is right)
    TCA0.SINGLE.PERBUF = IR_PERIOD;
    TCA0.SINGLE.INTCTRL = 0;                  // TODO: enable interrupts as needed
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm; // TODO: Probably only enable while sending

    // Enable pullup resistor on input PC0 and invert value
    PORTC.PIN0CTRL |= (PORT_PULLUPEN_bm | PORT_INVEN_bm);

    uint8_t slave_id = USERROW_USERROW0;
    uint8_t mode_bits = USERROW_USERROW1;

    bool mode_fancoil = mode_bits & 0x01;
    bool mode_iso_input = mode_bits & 0x02;

    modbus_client_init(slave_id, MB_BAUD, mode_iso_input ? &iso_input_state_ : NULL,
                       mode_fancoil ? setFancoil : NULL);

    sei();

    USART_DEBUG_SEND(slave_id);
    USART_DEBUG_SEND(mode_bits);
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
            if (!iso_input_state_) {
                USART_DEBUG_SEND('+');
            }
            iso_input_state_ = 1;
            iso_input_last_on_ = now;
        } else if ((now - iso_input_last_on_) > INPUT_READ_TIME_TICKS) {
            if (iso_input_state_) {
                USART_DEBUG_SEND('-');
            }
            // If we haven't read an ON value in INPUT_READ_TIME_TICKS, it's off
            iso_input_state_ = 0;
        }
    }
}

ISR(TCB0_INT_vect) {
    TCB0.INTFLAGS |= TCB_CAPT_bm;
    tick_++;
}
