#include <stdint.h>
#include <stdlib.h>

#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"

#include "modbus_client.h"

#define MB_BAUD 9600
#define INPUT_PIN_NUM PIN0_bm
#define TICK_PERIOD_US 500 // TODO: figure out the tick period
#define INPUT_READ_TIME_TICKS                                                                      \
    50 * 1000 / TICK_PERIOD_US // Attempt to detect AC for 50ms (3 cycles @ 60hz)

bool mode_fancoil_, mode_iso_input_;

volatile uint16_t tick_;
uint16_t iso_input_last_on_;
uint8_t iso_input_state_;

uint16_t getTick() {
    ATOMIC_BLOCK(ATOMIC_FORCEON) { return tick_; }
}

uint8_t setFancoil(bool cool_mode, uint8_t speed) {
    // TODO: Actually implement sending the IR codes
    return 1;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    // TODO: Update F_CLK_PER in platformio.ini if this changes, it probably will!!!
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 32 = 500khz

    // Use TCB0 as tick counter
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA =
        (TCB_ENABLE_bm | TCB_CLKSEL_DIV2_gc); // TODO: configure this frequency and set tick rate

    // TCA0 for IR transmission on WO2
    PORTMUX.TCAROUTEA |= PORTMUX_TCA02_bm; // Use alternate pin for WO2 (PB5 / pin 9)
    TCA0.SINGLE.CTRLB =
        TCA_SINGLE_CMP2EN_bm; // TODO: Configure waveform for dual slope PWM as appropriate
    TCA0.SINGLE.INTCTRL = 0;  // TODO: enable interrupts as needed
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm; // TODO: Configure clock division as needed

    // Enable pullup resistor on input PC0
    PORTC.PIN0CTRL |= PORT_PULLUPEN_bm;

    // TODO: Read from EEPROM
    uint8_t slave_id = 0x20;
    uint8_t mode_bits = 0xff;
    // uint8_t slave_id = USERROW_USERROW0;
    // uint8_t mode_bits = USERROW_USERROW1;

    mode_fancoil_ = mode_bits & 0x01;
    mode_iso_input_ = mode_bits & (0x01 << 1);

    modbus_client_init(slave_id, MB_BAUD, mode_iso_input_ ? &iso_input_state_ : NULL,
                       mode_fancoil_ ? setFancoil : NULL);

    sei();

    while (1) {
        wdt_reset();
        modbus_poll();

        uint16_t now = getTick();
        if (VPORTC.IN & INPUT_PIN_NUM) {
            // Any ON value read indicates an ON state
            iso_input_state_ = 1;
            iso_input_last_on_ = now;
        } else if ((now - iso_input_last_on_) > INPUT_READ_TIME_TICKS) {
            // If we haven't read an ON value in INPUT_READ_TIME_TICKS, it's off
            iso_input_state_ = 0;
        }
    }
}

ISR(TCB0_INT_vect) {
    TCB0.INTFLAGS |= TCB_CAPT_bm;
    tick_++;
}
