#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

#include "bme280_client.h"
#include "modbus_client.h"

#define POWER_PIN_NUM PIN2_bm
#define TACH_CLK F_CLK_PER / 2
#define MB_BAUD 9600
#define MB_SLAVE_ID 0x11
#define TICK_PERIOD_US 500
#define PHT_READ_INTERVAL_TICKS 2 * 1000 * 1000 / TICK_PERIOD_US

uint16_t last_pht_read_ticks_;
uint16_t tick_; // 0.5ms tick period
uint16_t curr_speed_;
volatile uint16_t tach_period_;

LastData last_data_;

void setSpeed(uint8_t speed) {
    if (speed == curr_speed_) {
        return;
    }

    TCA0.SPLIT.HCMP1 = speed; // Set PWM duty cycle

    //  PA4 for PWM, PB2 for power toggle
    if (speed == 0) {
        VPORTB.OUT &= ~POWER_PIN_NUM;
    } else {
        VPORTB.OUT |= POWER_PIN_NUM;
    }
}

uint16_t getLastTachRPM() {
    if (tach_period_ == 0) {
        return 0; // Invalid value
    }

    uint32_t tach_rpm = TACH_CLK * 60 / tach_period_;
    if (tach_rpm > UINT16_MAX) {
        return 0; // Invalid value -- impossible RPM
    }

    return (uint16_t)tach_rpm;
}

int main(void) {
    wdt_enable(WDTO_1S);

    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    // PB2 output for power toggle
    VPORTB.DIR |= POWER_PIN_NUM;

    // Tachometer using frequency measurement on PA5 / TCB0 W0
    // Expect speeds ~500-3000RPM
    PORTB.PIN5CTRL |= PORT_PULLUPEN_bm; // Enable pull up resistor
    TCB0.CTRLB = TCB_CNTMODE_FRQ_gc;    // Frequency count mode
    //TCB0.EVCTRL = TCB_CAPTEI_bm; // I had this set originally but think it isn't necessary?
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA =
        (TCB_ENABLE_bm | TCB_CLKSEL_DIV2_gc); // Configure tach frequency measurement @ ~156khz

    // Speed output on PA4, TCA WO4 (PWM).
    VPORTB.DIR |= PIN4_bm; // Output
    // Only single-slope supported with split-mode and we're using an output pin
    // that is only supported in split mode.
    // TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;  // Enable split mode
    TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP1EN_bm; // Enable WO4 (WO[n+3])
    TCA0.SPLIT.INTCTRL = TCA_SPLIT_LUNF_bm;  // Interrupt on low byte underflow for tick counter

    // High-byte runs at a bit less than 1.2khz to support full 8 bit range
    // 16e6 CPU / 64 prescaler / 256
    // Low byte runs at exactly 2khz to act as a tick counter
    // 16e6 CPU / 32 prescaler / 125
    TCA0.SPLIT.LPER = 125;

    TCA0.SPLIT.CTRLA = TCA_SPLIT_ENABLE_bm; // Run at ~1.2khz (16e6 / 64 prescaler / 256 range)

    bme280_init();
    last_pht_read_ticks_ = tick_;

    modbus_client_init(MB_SLAVE_ID, MB_BAUD, &last_data_, &curr_speed_);

    sei();

    while (1) {
        wdt_reset();
        last_data_.tach_rpm = getLastTachRPM();
        modbus_poll();

        if (tick_ - last_pht_read_ticks_ > PHT_READ_INTERVAL_TICKS) {
            bme280_get_latest(&last_data_.temp, &last_data_.humidity, &last_data_.pressure);
            last_pht_read_ticks_ = tick_;
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
        // If we overflowed, clear the overflow bit and set an invalid period
        TCB0.INTFLAGS &= ~TCB_OVF_bm;
        tach_period_ = 0;
    } else {
        tach_period_ = TCB0.CCMP; // reading CCMP clears interrupt flag
    }
}
