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
#define TICK_PERIOD_US 819UL
#define PHT_READ_INTERVAL_TICKS (2UL * 1000 * 1000 / TICK_PERIOD_US)
#define CTRL_INTERVAL_TICKS ((100UL * 1000) / TICK_PERIOD_US)
// When the filter needs replacing, the status LED will blink 2s on/off
// so we need to detect power state even when the LED may be off for >2s
// while the unit is powered on. Just hoping the light doesn't
// flash when the unit is powered off...
#define STATUS_ON_INTERVAL ((3UL * 1000 * 1000) / TICK_PERIOD_US)

uint16_t last_ctrl_action_ticks_;
uint16_t last_pht_read_ticks_;
uint16_t last_on_ticks_;
volatile uint16_t tick_;

uint16_t last_speed_;
LastData last_data_;

uint16_t getTick() {
    uint16_t tick;
    ATOMIC_BLOCK(ATOMIC_FORCEON) { tick = tick_; }
    return tick;
}

bool powerIsOn(uint16_t now) {
    if (VPORTA.IN & STATUS_LED_IN_PIN_NUM) {
        last_on_ticks_ = now;
        return true;
    } else if ((now - last_on_ticks_) >= STATUS_ON_INTERVAL) {
        // Keep rolling this forward to avoid overflow issues
        last_on_ticks_ = (now - STATUS_ON_INTERVAL);
        return false;
    } else {
        return true;
    }
}

void setSpeed(uint8_t speed, uint16_t now) {
    if (VPORTB.IN & CTRL_OUT_PIN_NUM) {
        // If we're currently "pressing" the button and enough time has passed since the press started,
        // unpress.
        if ((now - last_ctrl_action_ticks_) > CTRL_INTERVAL_TICKS) {
            VPORTB.OUT &= ~CTRL_OUT_PIN_NUM;
            last_ctrl_action_ticks_ = now;
        }
    } else if ((now - last_ctrl_action_ticks_) > (STATUS_ON_INTERVAL + CTRL_INTERVAL_TICKS) &&
               powerIsOn(now) != (last_speed_ > 0)) {
        // If the LED state does not match the desired speed state, press the button.
        // Note that we need to give enough time after the last control input for the LED
        // to turn off and for us to treat it as off.
        VPORTB.OUT |= CTRL_OUT_PIN_NUM;
        last_ctrl_action_ticks_ = now;
    }

    if (speed == last_speed_) {
        return;
    }

    TCA0.SPLIT.HCMP1 = (255 - speed);
    last_speed_ = speed;
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_16X_gc; // Divide main clock by 64 = 312500hz

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

    // High-byte runs at ~1.2khz to support full 8 bit range
    // 20e6 CPU / 16 prescaler / 4 timer scaler / 256
    TCA0.SPLIT.HCMP1 = 255;
    // Low byte runs at ~1.2khz to act as a tick counter
    // 20e6 CPU / 16 prescaler / 4 timer scaler / 256
    TCA0.SPLIT.LPER = 255;

    TCA0.SPLIT.CTRLA = TCA_SPLIT_ENABLE_bm | TCA_SPLIT_CLKSEL_DIV4_gc;

    bme280_init();

    uint16_t curr_speed = last_speed_;
    modbus_client_init(MB_SLAVE_ID, MB_BAUD, &last_data_, &curr_speed);

    last_ctrl_action_ticks_ = last_pht_read_ticks_ = getTick();

    sei();

    while (1) {
        wdt_reset();
        uint16_t now = getTick();
        last_data_.tach_rpm = powerIsOn(now);

        modbus_poll();

        setSpeed(curr_speed, now);

        if (now - last_pht_read_ticks_ > PHT_READ_INTERVAL_TICKS) {
            bme280_get_latest(&last_data_.temp, &last_data_.humidity, &last_data_.pressure);
        }
    }

    return 0;
}

ISR(TCA0_LUNF_vect) {
    TCA0.SPLIT.INTFLAGS |= TCA_SPLIT_LUNF_bm;
    tick_++;
}
