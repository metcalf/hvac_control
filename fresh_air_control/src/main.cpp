#include <stdint.h>

#include "avr/interrupt.h"
#include "avr/io.h"
#include "avr/wdt.h"
#include "util/atomic.h"

#include "bme280_client.h"
#include "modbus_client.h"
#include "usart_debug.h"

#define POWER_PIN_NUM PIN2_bm
#define TACH_CLK F_CLK_PER / 4UL
#define MB_BAUD 9600
#define MB_SLAVE_ID 0x11
#define TICK_PERIOD_US 819UL
#define PHT_READ_INTERVAL_TICKS (2UL * 1000 * 1000 / TICK_PERIOD_US)

uint16_t last_pht_read_ticks_;
volatile uint16_t tick_;
uint16_t last_speed_;
volatile uint16_t tach_period_;

LastData last_data_;

uint16_t getTick() {
    uint16_t tick;
    ATOMIC_BLOCK(ATOMIC_FORCEON) { tick = tick_; }
    return tick;
}

void setSpeed(uint8_t speed) {
    if (speed == last_speed_) {
        return;
    }

    // Set PWM duty cycle we invert the pin and invert
    // the duty cycle so that we can achieve 100% duty cycle
    // on a split timer. We don't need the 0% duty cycle since
    // we can just turn off the power pin.
    TCA0.SPLIT.HCMP1 = (255 - speed);

    //  PA4 for PWM, PB2 for power toggle
    if (speed == 0) {
        VPORTB.OUT &= ~POWER_PIN_NUM;
    } else {
        VPORTB.OUT |= POWER_PIN_NUM;
    }
    last_speed_ = speed;
}

uint16_t getLastTachRPM() {
    uint16_t period;
    ATOMIC_BLOCK(ATOMIC_FORCEON) { period = tach_period_; }

    if (period == 0) {
        return 0; // Invalid value
    }

    // Integer rounding division to convert period to frequency per minute
    uint32_t tach_rpm = (TACH_CLK * 60UL + period / 2) / period;
    if (tach_rpm > UINT16_MAX) {
        return UINT16_MAX; // Invalid value -- impossible RPM
    }

    return (uint16_t)tach_rpm;
}

void setupTachTimer() {
    // Tachometer using frequency measurement on PA5 / TCB0
    // Expect speeds ~500-3000RPM
    PORTA.PIN5CTRL = PORT_PULLUPEN_bm;             // Enable pull up resistor
    EVSYS.CHANNEL2 = EVSYS_CHANNEL2_PORTA_PIN5_gc; // Route pin PA5
    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL2_gc;   // to TCB0
    TCB0.CTRLB = TCB_CNTMODE_FRQ_gc;               // Frequency count mode
    TCB0.EVCTRL = TCB_CAPTEI_bm | TCB_EDGE_bm;     // Measure frequency between falling edge events
    TCB0.INTCTRL = TCB_OVF_bm | TCB_CAPT_bm;
    TCB0.CTRLA = (TCB_ENABLE_bm | TCB_CLKSEL_TCA0_gc);
}

int main(void) {
    wdt_enable(0x8); // 1 second (note the constants in avr/wdt are wrong for this chip)

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_16X_gc; // Divide main clock by 16 = 1.25mhz

    USART_DEBUG_INIT();
    setupTachTimer();

    // PB2 output for power toggle
    VPORTB.DIR |= POWER_PIN_NUM;

    // Speed output on PA4, TCA WO4 (PWM).
    VPORTA.DIR |= PIN4_bm; // Output
    PORTA.PIN4CTRL |= PORT_INVEN_bm;
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
    last_pht_read_ticks_ = getTick();

    uint16_t curr_speed = last_speed_;
    modbus_client_init(MB_SLAVE_ID, MB_BAUD, &last_data_, &curr_speed);

    sei();

    while (1) {
        wdt_reset();
        last_data_.tach_rpm = getLastTachRPM();
        modbus_poll();
        setSpeed(curr_speed);

        uint16_t now = getTick();
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

ISR(TCB0_INT_vect) {
    if (TCB0.INTFLAGS & TCB_OVF_bm) {
        // If we overflowed, clear both flags and set an invalid period
        TCB0.INTFLAGS |= TCB_OVF_bm;
        TCB0.INTFLAGS |= TCB_CAPT_bm;
        tach_period_ = 0;
    } else {
        tach_period_ = TCB0.CCMP; // reading CCMP clears interrupt flag
    }
}
