#include "rtc-rx8111.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_manager.h"

#define RTC_ADDR 0x32

/* Bug fixed in 2021/APR/6 */
/* Insert and Defined ALARM_AE */
/* Basic time and calendar register */
#define RX8111_SEC 0x10
#define RX8111_MIN 0x11
#define RX8111_HOUR 0x12
#define RX8111_WEEK 0x13
#define RX8111_DAY 0x14
#define RX8111_MONTH 0x15
#define RX8111_YEAR 0x16
#define RX8111_MIN_ALARM 0x17
#define RX8111_HOUR_ALARM 0x18
#define RX8111_WEEK_DAY_ALARM 0x19
#define RX8111_TIMER_COUNTER0 0x1A
#define RX8111_TIMER_COUNTER1 0x1B
#define RX8111_TIMER_COUNTER2 0x1C
#define RX8111_EXTENREG 0x1D
#define RX8111_FLAGREG 0x1E
#define RX8111_CTRLREG 0x1F

#define RX8111_TIMESTAMP_1_1000S 0x20
#define RX8111_TIMESTAMP_1_100S 0x21
#define RX8111_TIMESTAMP_SEC 0x22
#define RX8111_TIMESTAMP_MIN 0x23
#define RX8111_TIMESTAMP_HOUR 0x24
#define RX8111_TIMESTAMP_WEEK 0x25
#define RX8111_TIMESTAMP_DAY 0x26
#define RX8111_TIMESTAMP_MONTHS 0x27
#define RX8111_TIMESTAMP_YEAR 0x28
#define RX8111_STATUS_STAMP 0x29

#define RX8111_EVIN_SETTING 0x2B
#define RX8111_SEC_ALARM 0x2C
#define RX8111_TIMER_CONTROL 0x2D
#define RX8111_CMD_TRIG_CTRL 0x2E
#define RX8111_COMMAND_TRIGGER 0x2F

#define RX8111_PWR_SWITCH_CTRL 0x32
#define RX8111_STATUS_MONITOR 0x33
#define RX8111_TIME_STAMP_BUF_CTRL 0x34
#define RX8111_TIME_STAMP_TRIG_CTRL 0x35
#define RX8111_TIME_STAMP_DATA_STATUS 0x36

#define RX8111_EXT_TSEL0 BIT(0)
#define RX8111_EXT_TSEL1 BIT(1)
#define RX8111_EXT_ECP BIT(2)
#define RX8111_EXT_WADA BIT(3)
#define RX8111_EXT_TE BIT(4)
#define RX8111_EXT_USEL BIT(5)
#define RX8111_EXT_FSEL0 BIT(6)
#define RX8111_EXT_FSEL1 BIT(7)

#define RX8111_FLAG_FSTOPF BIT(0)
#define RX8111_FLAG_VLF BIT(1)
#define RX8111_FLAG_EVF BIT(2)
#define RX8111_FLAG_AF BIT(3)
#define RX8111_FLAG_TF BIT(4)
#define RX8111_FLAG_UF BIT(5)
#define RX8111_FLAG_PORF BIT(7)

#define RX8111_CTRL_STOP BIT(0)
#define RX8111_CTRL_EIE BIT(2)
#define RX8111_CTRL_AIE BIT(3)
#define RX8111_CTRL_TIE BIT(4)
#define RX8111_CTRL_UIE BIT(5)

#define RX8111_EVIN_EOVEN BIT(1)
#define RX8111_EVIN_EPRUP_SEL0 BIT(2)
#define RX8111_EVIN_EPRUP_SEL1 BIT(3)
#define RX8111_EVIN_EPRDW_SEL BIT(4)
#define RX8111_EVIN_ET0 BIT(5)
#define RX8111_EVIN_ET1 BIT(6)
#define RX8111_EVIN_EHL BIT(7)

#define RX8111_TIMER_CTRL_TSTP BIT(0)
#define RX8111_TIMER_CTRL_TMPIN BIT(1)
#define RX8111_TIMER_CTRL_TBKE BIT(2)
#define RX8111_TIMER_CTRL_TBKON BIT(3)

#define RX8111_CMD_TRIG_DUMMY0 BIT(0)
#define RX8111_CMD_TRIG_DUMMY1 BIT(1)
#define RX8111_CMD_TRIG_DUMMY2 BIT(2)
#define RX8111_CMD_TRIG_DUMMY3 BIT(3)
#define RX8111_CMD_TRIG_DUMMY4 BIT(4)
#define RX8111_CMD_TRIG_DUMMY5 BIT(5)
#define RX8111_CMD_TRIG_DUMMY6 BIT(6)
#define RX8111_CMD_TRIG_DUMMY7 BIT(7)

#define RX8111_PSC_SMP_TSEL0 BIT(0)
#define RX8111_PSC_SMP_TSEL1 BIT(1)
#define RX8111_PSC_SMP_SWSEL0 BIT(2)
#define RX8111_PSC_SMP_SWSEL1 BIT(3)
#define RX8111_PSC_SMP_INIEN BIT(6)
#define RX8111_PSC_SMP_CHGEN BIT(7)

#define RX8111_PSC_SMP_CHGEN BIT(7)

#define RX8111_STAT_M_FVLOW BIT(1)
#define RX8111_STAT_M_FVCMP BIT(3)
#define RX8111_STAT_M_EVINMON BIT(6)
/* Insert and Defined ALARM_AE */
#define RX8111_ALARM_AE BIT(7)

static const char *TAG = "RTC";

/*!
      @brief  Convert a binary coded decimal value to binary. RTC stores
    time/date values as BCD.
      @param val BCD value
      @return Binary value
  */
static uint8_t bcd2bin(uint8_t val) { return (val & 0x0f) + (val >> 4) * 10; }
/*!
      @brief  Convert a binary value to BCD format for the RTC registers
      @param val Binary value
      @return BCD value
  */
static uint8_t bin2bcd(uint8_t val) { return ((val / 10) << 4) + val % 10; }

static esp_err_t rtc_rx8111_i2c_read(uint32_t reg, uint8_t *buffer, uint16_t size) {
    return i2c_manager_read(I2C_NUM_0, RTC_ADDR, reg, buffer, size);
}
static esp_err_t rtc_rx8111_i2c_write(uint32_t reg, const uint8_t *buffer, uint16_t size) {
    return i2c_manager_write(I2C_NUM_0, RTC_ADDR, reg, buffer, size);
}

static esp_err_t rtc_rx8111_i2c_write_reg(uint32_t reg, const uint8_t value) {
    return i2c_manager_write(I2C_NUM_0, RTC_ADDR, reg, &value, 1);
}

//----------------------------------------------------------------------
// rx8010_get_time()
// gets the current time from the rx8010 registers
//
//----------------------------------------------------------------------
esp_err_t rtc_rx8111_get_time(struct tm *dt) {
    uint8_t date[7];
    esp_err_t err = rtc_rx8111_i2c_read(RX8111_SEC, date, sizeof(date));
    if (err != ESP_OK) {
        return err;
    }

    dt->tm_sec = bcd2bin(date[RX8111_SEC - RX8111_SEC] & 0x7f);
    dt->tm_min = bcd2bin(date[RX8111_MIN - RX8111_SEC] & 0x7f);
    dt->tm_hour = bcd2bin(date[RX8111_HOUR - RX8111_SEC] & 0x3f);
    dt->tm_mday = bcd2bin(date[RX8111_DAY - RX8111_SEC] & 0x3f);
    dt->tm_mon = bcd2bin(date[RX8111_MONTH - RX8111_SEC] & 0x1f) - 1;
    dt->tm_year = bcd2bin(date[RX8111_YEAR - RX8111_SEC]) + 100;
    dt->tm_wday = __builtin_ffs(date[RX8111_WEEK - RX8111_SEC] & 0x7f);

    return ESP_OK;
}

//----------------------------------------------------------------------
// rx8010_set_time()
// Sets the current time in the rx8010 registers
//
// BUG: The HW assumes every year that is a multiple of 4 to be a leap
// year. Next time this is wrong is 2100, which will not be a leap year
//
// Note: If STOP is not set/cleared, the clock will start when the seconds
//       register is written
//
//----------------------------------------------------------------------
esp_err_t rtc_rx8111_set_time(struct tm *dt) {
    uint8_t date[7];
    esp_err_t err;
    if ((dt->tm_year < 100) || (dt->tm_year > 199)) {
        return ESP_ERR_INVALID_ARG;
    }

    // set STOP bit to "1" to prevent imter update in time setting.
    // The original driver read the value of control first and only updated this bit
    // but we never use the other functions so we can leave them at zero.
    err = rtc_rx8111_i2c_write_reg(RX8111_CTRLREG, RX8111_CTRL_STOP);
    if (err != ESP_OK) {
        return err;
    }

    date[RX8111_SEC - RX8111_SEC] = bin2bcd(dt->tm_sec);
    date[RX8111_MIN - RX8111_SEC] = bin2bcd(dt->tm_min);
    date[RX8111_HOUR - RX8111_SEC] = bin2bcd(dt->tm_hour);
    date[RX8111_DAY - RX8111_SEC] = bin2bcd(dt->tm_mday);
    date[RX8111_MONTH - RX8111_SEC] = bin2bcd(dt->tm_mon + 1);
    date[RX8111_YEAR - RX8111_SEC] = bin2bcd(dt->tm_year - 100);
    date[RX8111_WEEK - RX8111_SEC] = bin2bcd(1 << dt->tm_wday);

    err = rtc_rx8111_i2c_write(RX8111_SEC, date, sizeof(date));
    if (err != ESP_OK) {
        return err;
    }

    // Clear stop.
    err = rtc_rx8111_i2c_write_reg(RX8111_CTRLREG, 0);
    if (err != ESP_OK) {
        return err;
    }

    return 0;
}

// Clear VLF flag (and others)
static int rx8111_try_clear_flags() {
    esp_err_t err;

    for (int i = 0; i < 300; i++) {
        err = rtc_rx8111_i2c_write_reg(RX8111_FLAGREG, 0x00);
        if (err != ESP_OK) {
            return err;
        }

        uint8_t flag;
        err = rtc_rx8111_i2c_read(RX8111_FLAGREG, &flag, 1);
        if (err != ESP_OK) {
            return err;
        }

        if (!(flag & RX8111_FLAG_VLF)) {
            return ESP_OK; // VLF flag is zero indicating ready to init
        }

        ESP_LOGD(TAG, "Flags: %02d", flag);

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGE(TAG, "Timeout clearing VLF flag");

    return ESP_FAIL;
}

esp_err_t rtc_rx8111_init_client(bool *hasTime) {
    esp_err_t err;

    uint8_t flag;
    err = rtc_rx8111_i2c_read(RX8111_FLAGREG, &flag, 1);
    if (err != ESP_OK) {
        return err;
    }

    if ((flag & RX8111_FLAG_VLF)) {
        // Before continuing, wait for the oscillator to stabilize (indicated by
        // successfully clearing the VLF flag)
        *hasTime = false;
        err = rx8111_try_clear_flags();
        if (err != ESP_OK) {
            return err;
        }
    } else {
        // If the VLF flag is zero, this indicates the RTC returned from battery backup
        // normally. We could skip the rest of the initialization at this point but I
        // could see that resulting in edge cases when we change the init sequence without
        // removing the battery.
        *hasTime = true;
    }

    err =
        rtc_rx8111_i2c_write_reg(RX8111_PWR_SWITCH_CTRL,
                                 // Internally switch off VDD for 128ms every second to detect power
                                 // state. This is the default.
                                 RX8111_PSC_SMP_TSEL1 |
                                     // Enable non-rechargeable backup battery operation
                                     RX8111_PSC_SMP_INIEN); //0x32, bit2 = 1
    if (err != ESP_OK) {
        return err;
    }

    err = rtc_rx8111_i2c_write_reg(
        RX8111_EXTENREG,
        0 |
            // Since we're not using the frequency output feature, set both registers to `1` per
            // datasheet
            RX8111_EXT_FSEL0 | RX8111_EXT_FSEL1 |
            // Since we're not using the wakeup interrupt feature, TSEL1 should be set and the
            // other relevant bits should be zero per datasheet.
            RX8111_EXT_TSEL1);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}
