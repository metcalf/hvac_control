#include <stdint.h>

#include "avr/interrupt.h"
#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"

#include "bme280_client.h"
#include "modbus_client.h"
#include "usart_debug.h"

#define STATUS_LED_IN_PIN_NUM PIN5_bm // PORTA
#define CTRL_OUT_PIN_NUM PIN2_bm      // PORTB
#define MB_BAUD 9600
#define MB_SLAVE_ID 0x11
#define TICK_PERIOD_US 500UL
#define PHT_READ_INTERVAL_TICKS (2UL * 1000 * 1000 / TICK_PERIOD_US)
#define CTRL_INTERVAL_TICKS ((100 * 1000) / TICK_PERIOD_US)

uint16_t last_ctrl_action_ticks_;
uint16_t last_pht_read_ticks_;
volatile uint16_t tick_; // 0.5ms tick period
uint16_t last_speed_;
volatile uint16_t tach_period_;

LastData last_data_;

uint16_t getTick() {
    uint16_t tick;
    ATOMIC_BLOCK(ATOMIC_FORCEON) { tick = tick_; }
    return tick;
}

void setSpeed(uint8_t speed, uint16_t now) {
    // Don't do anything with the controls if we've handled them recently
    if ((now - last_ctrl_action_ticks_) > CTRL_INTERVAL_TICKS) {
        if (VPORTB.OUT & CTRL_OUT_PIN_NUM) {
            // If we're currently pressing the button, unpress it
            VPORTB.OUT &= ~(CTRL_OUT_PIN_NUM);
            last_ctrl_action_ticks_ = now;
        } else if (!!(VPORTA.IN & STATUS_LED_IN_PIN_NUM) != (last_speed_ != 0)) {
            // If the LED state does not match the desired speed state, press the button.
            // Note that we only read this state after CTRL_INTERVAL_TICKS have elapsed since
            // releasing the button to give the host time to respond.
            VPORTB.OUT |= CTRL_OUT_PIN_NUM;
            last_ctrl_action_ticks_ = now;
        }
    }

    if (speed == last_speed_) {
        return;
    }

    TCA0.SPLIT.HCMP1 = speed;
    last_speed_ = speed;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    USART_DEBUG_INIT();

    VPORTB.DIR |= CTRL_OUT_PIN_NUM;

    // Invert status input (LED on -> opto conducts to ground)
    PORTA.PIN5CTRL |= (PORT_INVEN_bm | PORT_PULLUPEN_bm);

    // Speed output on PA4, TCA WO4 (PWM).
    VPORTA.DIR |= PIN4_bm; // Output
    // Only single-slope supported with split-mode and we're using an output pin
    // that is only supported in split mode.
    // TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;  // Enable split mode
    TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP1EN_bm; // Enable WO4 (WO[n+3])
    TCA0.SPLIT.INTCTRL = TCA_SPLIT_LUNF_bm;  // Interrupt on low byte underflow for tick counter
    TCA0.SPLIT.HCMP1 = 255;

    // High-byte runs at a bit less than 1.2khz to support full 8 bit range
    // 20e6 CPU / 64 prescaler / 256
    // Low byte runs at ~2khz to act as a tick counter
    // 20e6 CPU / 64 prescaler / 156
    TCA0.SPLIT.LPER = 156;

    TCA0.SPLIT.CTRLA = TCA_SPLIT_ENABLE_bm; // Run at ~1.2khz (16e6 / 64 prescaler / 256 range)

    bme280_init();

    uint16_t curr_speed = last_speed_;
    modbus_client_init(MB_SLAVE_ID, MB_BAUD, &last_data_, &curr_speed);

    last_ctrl_action_ticks_ = last_pht_read_ticks_ = getTick();

    sei();

    while (1) {
        wdt_reset();
        modbus_poll();
        uint16_t now = getTick();

        setSpeed(curr_speed, now);

        if (now - last_pht_read_ticks_ > PHT_READ_INTERVAL_TICKS) {
            bme280_get_latest(&last_data_.temp, &last_data_.humidity, &last_data_.pressure);
            last_pht_read_ticks_ = now;
        }
    }

    return 0;
}

ISR(TCA0_LUNF_vect) {
    TCA0.SPLIT.INTFLAGS |= TCA_SPLIT_LUNF_bm;
    tick_++;
}
