#include "UIManager.h"

#include <chrono>
#include <cmath>

#include "driver/gpio.h"
#include "esp_log.h"
#include "ui_utils.h"

using HVACState = ControllerDomain::HVACState;
using Config = ControllerDomain::Config;

#define TEMP_LIMIT_ROLLER_START 64
#define MIN_HEAT_DEG 50
#define MAX_COOL_DEG 99
#define MIN_HEAT_COOL_DELTA_DEG 2
#define MIN_HEAT_COOL_DELTA_C ABS_F_TO_C(MIN_HEAT_COOL_DELTA_DEG)

static const char *TAG = "UI";

lv_obj_t **wifiTextareas[] = {
    &ui_wifi_ssid,
    &ui_wifi_password,
    &ui_log_name,
};
lv_obj_t **wifiContainers[] = {
    &ui_ssid_container,
    &ui_password_container,
    &ui_log_name_container,
};

size_t nWifiTextareas = sizeof(wifiTextareas) / sizeof(wifiTextareas[0]);

uint8_t getTempOffsetTenthDeg(lv_obj_t *roller) { return lv_roller_get_selected(roller) - 50; }

double getTempOffsetF(lv_obj_t *roller) { return ((double)getTempOffsetTenthDeg(roller)) / 10; }

uint16_t tempOffsetRollerOpt(double tempOffsetC) { return REL_C_TO_F(tempOffsetC) * 10 + 50; }

void updateClk() {
    struct tm dt;
    time_t nowUTC = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    localtime_r(&nowUTC, &dt);

    if (dt.tm_year < 120) {
        // Before 2020--time must be invalid
        lv_label_set_text(ui_clock_value, "--:-- ");
    } else {
        int hr = dt.tm_hour % 12;
        if (hr == 0) {
            hr = 12;
        }
        lv_label_set_text_fmt(ui_clock_value, "%02d:%02d%c", hr, dt.tm_min,
                              dt.tm_hour > 11 ? 'P' : 'A');
    }
}

void updateClkCb(lv_timer_t *timer) { updateClk(); }

void wifiTextareaEventCb(lv_event_t *e) {
    ((UIManager *)lv_event_get_user_data(e))->eWifiTextarea(e);
}

void heatRollerChangeCb(lv_event_t *e) {
    UIManager::eventsInst()->handleTempRollerChange(true, lv_event_get_target(e),
                                                    (lv_obj_t *)lv_event_get_user_data(e));
}

void coolRollerChangeCb(lv_event_t *e) {
    UIManager::eventsInst()->handleTempRollerChange(false, (lv_obj_t *)lv_event_get_user_data(e),
                                                    lv_event_get_target(e));
}

void UIManager::bootDone() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    booted_ = true;
    _ui_screen_change(&ui_Home, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Home_screen_init);
    lv_disp_trig_activity(NULL);
    xSemaphoreGive(mutex_);
}

void UIManager::bootErr(const char *msg) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_label_set_text(ui_boot_error_text, msg);

    objSetVisibility(false, ui_boot_message);
    objSetVisibility(true, ui_boot_error_heading);
    objSetVisibility(true, ui_boot_error_text);
    xSemaphoreGive(mutex_);
}

void UIManager::sendACOverrideEvent(ACOverride override) {
    Event evt{EventType::ACOverride, EventPayload{.acOverride = override}};
    eventCb_(evt);
}

void UIManager::sendPowerEvent(bool on) {
    Event evt{EventType::SetSystemPower, EventPayload{.systemPower = on}};
    eventCb_(evt);
}

int UIManager::getHeatRollerValueDeg(lv_obj_t *roller) {
    return MIN_HEAT_DEG + lv_roller_get_selected(roller);
}

int UIManager::getCoolRollerValueDeg(lv_obj_t *roller) {
    return minCoolDeg_ + lv_roller_get_selected(roller);
}

double UIManager::getHeatRollerValueC(lv_obj_t *roller) {
    return ABS_F_TO_C(getHeatRollerValueDeg(roller));
}
double UIManager::getCoolRollerValueC(lv_obj_t *roller) {
    return ABS_F_TO_C(getCoolRollerValueDeg(roller));
}

void UIManager::setupTempRollerPair(lv_obj_t *heatRoller, lv_obj_t *coolRoller) {
    setupTempRoller(heatRoller, MIN_HEAT_DEG, maxHeatDeg_);
    setupTempRoller(coolRoller, minCoolDeg_, MAX_COOL_DEG);

    lv_obj_add_event_cb(heatRoller, heatRollerChangeCb, LV_EVENT_VALUE_CHANGED, coolRoller);
    lv_obj_add_event_cb(coolRoller, coolRollerChangeCb, LV_EVENT_VALUE_CHANGED, heatRoller);
}

// Maintain the minimum separation between heat/cool setpoints as rollers update
void UIManager::handleTempRollerChange(bool heatChanged, lv_obj_t *heatRoller,
                                       lv_obj_t *coolRoller) {
    int heatDeg = getHeatRollerValueDeg(heatRoller);
    int coolDeg = getCoolRollerValueDeg(coolRoller);

    if (coolDeg - heatDeg < MIN_HEAT_COOL_DELTA_DEG) {
        if (heatChanged) {
            lv_roller_set_selected(coolRoller, (heatDeg + MIN_HEAT_COOL_DELTA_DEG) - minCoolDeg_,
                                   LV_ANIM_ON);
        } else {
            lv_roller_set_selected(heatRoller, (coolDeg - MIN_HEAT_COOL_DELTA_DEG) - MIN_HEAT_DEG,
                                   LV_ANIM_ON);
        }
    }
}

void UIManager::setupTempRoller(lv_obj_t *roller, uint8_t minDeg, uint8_t maxDeg) {

    assert(maxDeg > minDeg);
    assert(maxDeg < 100);

    char opts[300] = "";
    size_t pos = 0;
    for (uint8_t currDeg = minDeg; currDeg < maxDeg; currDeg++) {
        pos += snprintf(opts + pos, sizeof(opts) - pos, "%u\n", currDeg);
    }
    sprintf(opts + pos, "%u", maxDeg); // No trailing newline on the last element

    lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
}

void UIManager::eFanOverride() {
    uint16_t fanSpeedPct = lv_roller_get_selected(ui_Fan_override_speed) * 10;
    uint8_t fanSpeed = (uint8_t)((fanSpeedPct * 255 + (100 / 2)) / 100);

    Event evt{
        EventType::FanOverride,
        EventPayload{.fanOverride = {
                         .speed = fanSpeed,
                         .timeMins = (uint16_t)(lv_roller_get_selected(ui_Fan_override_hrs) * 60 +
                                                lv_roller_get_selected(ui_Fan_override_mins)),
                     }}};
    eventCb_(evt);
}

void UIManager::eThermotatOverride() {
    Event evt{EventType::TempOverride,
              EventPayload{.tempOverride = {
                               .heatC = getHeatRollerValueC(ui_Heat_override_setpoint),
                               .coolC = getCoolRollerValueC(ui_Cool_override_setpoint),
                           }}};
    eventCb_(evt);
}

// This isn't currently implemented since it requires more complicated
// state tracking with the AC on/off override and may not be necessary at all
void UIManager::eUseAC() {
    sendACOverrideEvent(ACOverride::Force);

    // Hide "use A/C" and show "stop A/C"
    // objSetVisibility(false, ui_use_ac_button);
    // objSetVisibility(true, ui_stop_ac_button);
}

void UIManager::eStopAC() {
    sendACOverrideEvent(ACOverride::Stop);

    // Hide "stop A/C" and show "allow A/C"
    objSetVisibility(false, ui_stop_ac_button);
    objSetVisibility(true, ui_allow_ac_button);
}

void UIManager::eAllowAC() {
    sendACOverrideEvent(ACOverride::Normal);

    // Hide "allow A/C" and show "stop A/C"
    objSetVisibility(false, ui_allow_ac_button);
    objSetVisibility(true, ui_stop_ac_button);
}

void UIManager::eSystemOff() {
    sendPowerEvent(false);
    setSystemPower(false);
}

void UIManager::eSystemOn() {
    sendPowerEvent(true);
    setSystemPower(true);
}

void UIManager::eTargetCO2() {
    co2Target_ = 800 + lv_roller_get_selected(ui_co2_target) * 50;
    Event evt{EventType::SetCO2Target, EventPayload{.co2Target = co2Target_}};
    eventCb_(evt);

    lv_label_set_text_fmt(ui_co2_target_value, "%u", co2Target_);
}

void UIManager::eSchedule() {
    currSchedules_[0] = {
        .heatC = getHeatRollerValueC(ui_Day_heat_setpoint),
        .coolC = getHeatRollerValueC(ui_Day_cool_setpoint),
        .startHr = (uint8_t)lv_roller_get_selected(ui_Day_hr),
        .startMin = (uint8_t)lv_roller_get_selected(ui_Day_min),
    };
    currSchedules_[1] = {
        .heatC = getHeatRollerValueC(ui_Night_heat_setpoint),
        .coolC = getHeatRollerValueC(ui_Night_cool_setpoint),
        .startHr = (uint8_t)(lv_roller_get_selected(ui_Night_hr) + 12),
        .startMin = (uint8_t)lv_roller_get_selected(ui_Night_min),
    };

    Event evt{EventType::SetSchedule, EventPayload{.schedules = {
                                                       currSchedules_[0],
                                                       currSchedules_[1],
                                                   }}};
    eventCb_(evt);
}

void UIManager::eHomeLoadStart() {
    ESP_LOGD(TAG, "homeLoadStart");
    msgMgr_->startScrollTimer();
}

void UIManager::eHomeUnloadStart() {
    ESP_LOGD(TAG, "homeUnloadStart");
    msgMgr_->pauseScrollTimer();
}

void UIManager::eCO2LoadStart() {
    ESP_LOGD(TAG, "co2LoadStart");
    lv_roller_set_selected(ui_co2_target, (co2Target_ - 800) / 50, LV_ANIM_OFF);
}

void UIManager::eThermostatLoadStart() {
    ESP_LOGD(TAG, "thermostatLoadStart");
    lv_roller_set_selected(ui_Heat_override_setpoint, currHeatDeg_ - MIN_HEAT_DEG, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Cool_override_setpoint, currCoolDeg_ - minCoolDeg_, LV_ANIM_OFF);
}

void UIManager::eScheduleLoadStart() {
    ESP_LOGD(TAG, "scheduleLoadStart");
    lv_roller_set_selected(ui_Day_heat_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[0].heatC)) - MIN_HEAT_DEG,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_cool_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[0].coolC)) - minCoolDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_hr, currSchedules_[0].startHr, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_min, currSchedules_[0].startMin, LV_ANIM_OFF);

    lv_roller_set_selected(ui_Night_heat_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[1].heatC)) - MIN_HEAT_DEG,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_cool_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[1].coolC)) - minCoolDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_hr, currSchedules_[1].startHr - 12, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_min, currSchedules_[1].startMin, LV_ANIM_OFF);
}

void UIManager::eSaveEquipmentSettings() {
    equipment_.heatType = Config::HVACType(lv_dropdown_get_selected(ui_heat_type));
    equipment_.coolType = Config::HVACType(lv_dropdown_get_selected(ui_cool_type));
    equipment_.hasMakeupDemand = lv_obj_has_state(ui_makeup_air_switch, LV_STATE_CHECKED);

    updateUIForEquipment();

    Event evt{
        EventType::SetEquipment,
        EventPayload{.equipment = equipment_},
    };

    eventCb_(evt);

    _ui_screen_change(&ui_Settings, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Settings_screen_init);
}

void UIManager::eSaveTempLimits() {
    double maxHeatC = ABS_F_TO_C(maxHeatDeg_);
    double minCoolC = ABS_F_TO_C(minCoolDeg_);

    updateTempLimits(lv_roller_get_selected(ui_heat_limit) + TEMP_LIMIT_ROLLER_START,
                     lv_roller_get_selected(ui_cool_limit) + TEMP_LIMIT_ROLLER_START);
    Event evt{EventType::SetTempLimits, EventPayload{.tempLimits = {
                                                         .maxHeatC = maxHeatC,
                                                         .minCoolC = minCoolC,
                                                     }}};
    eventCb_(evt);

    // Update schedules to confirm to the new limits
    bool scheduleChanged = false;
    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        Config::Schedule *schedule = &currSchedules_[i];
        if (schedule->coolC < minCoolC) {
            schedule->coolC = minCoolC;
            if ((schedule->coolC - schedule->heatC) < MIN_HEAT_COOL_DELTA_C) {
                schedule->heatC = schedule->coolC - MIN_HEAT_COOL_DELTA_C;
            }
            scheduleChanged = true;
        }
        if (schedule->heatC > maxHeatC) {
            schedule->heatC = maxHeatC;
            if ((schedule->coolC - schedule->heatC) < MIN_HEAT_COOL_DELTA_C) {
                schedule->coolC = schedule->heatC + MIN_HEAT_COOL_DELTA_C;
            }
            scheduleChanged = true;
        }
    }
    if (scheduleChanged) {
        Event scheduleEvt{EventType::SetSchedule, EventPayload{.schedules = {
                                                                   currSchedules_[0],
                                                                   currSchedules_[1],
                                                               }}};
        eventCb_(scheduleEvt);
    }
}

void UIManager::eSaveTempOffsets() {
    inTempOffsetC_ = REL_F_TO_C(getTempOffsetF(ui_indoor_offset));
    outTempOffsetC_ = REL_F_TO_C(getTempOffsetF(ui_fan_offset));

    Event evt{
        EventType::SetTempOffsets,
        EventPayload{.tempOffsets =
                         {
                             .inTempOffsetC = inTempOffsetC_,
                             .outTempOffsetC = outTempOffsetC_,
                         }},
    };
    eventCb_(evt);
}

void UIManager::eSaveWifiSettings() {
    // NB: strncat used to ensure null termination
    strncat(wifi_.ssid, lv_textarea_get_text(ui_wifi_ssid), sizeof(wifi_.ssid) - 1);
    strncat(wifi_.logName, lv_textarea_get_text(ui_log_name), sizeof(wifi_.logName) - 1);

    const char *pswd = lv_textarea_get_text(ui_wifi_password);
    if (strlen(pswd) > 0) {
        strncat(wifi_.password, pswd, sizeof(wifi_.password) - 1);
    }

    Event evt{
        EventType::SetWifi,
        EventPayload{.wifi = wifi_},
    };

    eventCb_(evt);
}

void UIManager::eEquipmentSettingsLoadStart() {
    lv_dropdown_set_selected(ui_heat_type, static_cast<uint16_t>(equipment_.heatType));
    lv_dropdown_set_selected(ui_cool_type, static_cast<uint16_t>(equipment_.coolType));
    if (equipment_.hasMakeupDemand) {
        lv_obj_add_state(ui_makeup_air_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_makeup_air_switch, LV_STATE_CHECKED);
    }
}

void UIManager::eTempLimitsLoadStart() {
    lv_roller_set_selected(ui_heat_limit, maxHeatDeg_ - TEMP_LIMIT_ROLLER_START, LV_ANIM_OFF);
    lv_roller_set_selected(ui_cool_limit, minCoolDeg_ - TEMP_LIMIT_ROLLER_START, LV_ANIM_OFF);
}

void UIManager::eTempOffsetsLoadStart() {
    lv_roller_set_selected(ui_indoor_offset, tempOffsetRollerOpt(inTempOffsetC_), LV_ANIM_OFF);
    lv_roller_set_selected(ui_fan_offset, tempOffsetRollerOpt(outTempOffsetC_), LV_ANIM_OFF);
    eTempOffsetChanged(); // Trigger update of current temp values
}

void UIManager::eWifiSettingsLoadStart() {
    lv_textarea_set_text(ui_wifi_ssid, wifi_.ssid);
    lv_textarea_set_text(ui_wifi_password, "");
    lv_textarea_set_text(ui_log_name, wifi_.logName);
}

void UIManager::eTempOffsetChanged() {
    lv_label_set_text_fmt(ui_indoor_offset_label, "%.1f°",
                          ABS_C_TO_F(currInTempC_) + getTempOffsetF(ui_indoor_offset));
    lv_label_set_text_fmt(ui_fan_offset_label, "%.1f°",
                          ABS_C_TO_F(currOutTempC_) + getTempOffsetF(ui_fan_offset));
}

void UIManager::eWifiTextarea(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(ui_wifi_keyboard, ta);
        objSetVisibility(true, ui_wifi_keyboard);

        for (int i = 0; i < nWifiTextareas; i++) {
            objSetVisibility(ta == *wifiTextareas[i], *wifiContainers[i]);
        }
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL || code == LV_EVENT_DEFOCUSED) {
        if (code == LV_EVENT_DEFOCUSED) {
            lv_keyboard_set_textarea(ui_wifi_keyboard, NULL);
        }

        objSetVisibility(false, ui_wifi_keyboard);

        for (int i = 0; i < nWifiTextareas; i++) {
            objSetVisibility(true, *wifiContainers[i]);
        }

        lv_indev_reset(NULL, ta); /*To forget the last clicked object to make it focusable again*/
    }
}

void UIManager::updateTempLimits(uint8_t maxHeatDeg, uint8_t minCoolDeg) {
    maxHeatDeg_ = maxHeatDeg;
    minCoolDeg_ = minCoolDeg;

    setupTempRollerPair(ui_Heat_override_setpoint, ui_Cool_override_setpoint);
    setupTempRollerPair(ui_Day_heat_setpoint, ui_Day_cool_setpoint);
    setupTempRollerPair(ui_Night_heat_setpoint, ui_Night_cool_setpoint);
}

void UIManager::updateUIForEquipment() {
    // Hide AC buttons if we don't have active cooling
    objSetFlag(equipment_.coolType == Config::HVACType::None, ui_ac_button_container,
               LV_OBJ_FLAG_HIDDEN);
}

void UIManager::onCancelMsg(uint8_t msgID) {
    Event evt{EventType::MsgCancel, EventPayload{.msgID = msgID}};
    eventCb_(evt);
}

// I ran out of allowed widgets in the free version of Squareline so this is
// where I add new stuff until I come up with a better answer.
void UIManager::initExtraWidgets() {
    // TODO: Actually provide firmware version
    firmwareVersionLabel_ = lv_label_create(ui_Settings);
    lv_obj_remove_style_all(firmwareVersionLabel_);
    lv_obj_set_width(firmwareVersionLabel_, 300);
    lv_obj_set_height(firmwareVersionLabel_, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(firmwareVersionLabel_, LV_ALIGN_CENTER);
    lv_label_set_text(firmwareVersionLabel_, "");
    lv_obj_set_style_text_font(firmwareVersionLabel_, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
}

UIManager::UIManager(ControllerDomain::Config config, size_t nMsgIds, eventCb_t eventCb)
    : eventCb_(eventCb) {
    mutex_ = xSemaphoreCreateMutex();

    inTempOffsetC_ = config.inTempOffsetC;
    outTempOffsetC_ = config.outTempOffsetC;
    co2Target_ = config.co2Target;
    equipment_ = config.equipment;
    wifi_ = config.wifi;

    ui_init();
    initExtraWidgets();

    sleepMgr_ = new SleepManager(ui_Home, GPIO_NUM_48);

    setInTempC(std::nan(""));
    setOutTempC(std::nan(""));

    for (int i = 0; i < nWifiTextareas; i++) {
        lv_obj_add_event_cb(*wifiTextareas[i], wifiTextareaEventCb, LV_EVENT_ALL, this);
    }

    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        currSchedules_[i] = config.schedules[i];
    }
    updateTempLimits(std::round(ABS_C_TO_F(config.maxHeatC)),
                     std::round(ABS_C_TO_F(config.minCoolC)));

    lv_label_set_text_fmt(ui_co2_target_value, "%u", config.co2Target);

    updateUIForEquipment();

    lv_obj_del(
        ui_message_container); // Delete the message container we created with Squareline for design purposes
    msgMgr_ = new MessageManager(nMsgIds, ui_Footer, &ui_font_MaterialSymbols24,
                                 new cancelCbFn_t([this](uint8_t msgID) { onCancelMsg(msgID); }));

    clkTimer_ = lv_timer_create(updateClkCb, 1000, this);
    updateClk();
}

uint32_t UIManager::handleTasks() {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    if (booted_) {
        sleepMgr_->update();
    }
    auto rv = lv_timer_handler();
    xSemaphoreGive(mutex_);
    return rv;
}

void UIManager::setHumidity(double h) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (std::isnan(h)) {
        lv_label_set_text(ui_Humidity_value, "--%");
    } else {
        lv_label_set_text_fmt(ui_Humidity_value, "%u%%", std::min(99U, (uint)std::round(h)));
    }

    xSemaphoreGive(mutex_);
}

void UIManager::setCurrentFanSpeed(uint8_t speed) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (speed == 0) {
        lv_label_set_text(ui_Fan_value, "OFF");
    } else {
        lv_label_set_text_fmt(ui_Fan_value, "%u%%", (uint)std::round(speed / 255.0 * 100));
    }
    xSemaphoreGive(mutex_);
}

void UIManager::setOutTempC(double tc) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    currOutTempC_ = tc;
    if (std::isnan(tc)) {
        lv_label_set_text(ui_Out_temp_value, "--°");
    } else {
        lv_label_set_text_fmt(ui_Out_temp_value, "%u°", (uint)std::round(ABS_C_TO_F(tc)));
    }
    xSemaphoreGive(mutex_);
}

void UIManager::setInTempC(double tc) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    currInTempC_ = tc;
    if (std::isnan(tc)) {
        lv_label_set_text(ui_Indoor_temp_value, "--");
    } else {
        lv_label_set_text_fmt(ui_Indoor_temp_value, "%u", (uint)std::round(ABS_C_TO_F(tc)));
    }
    xSemaphoreGive(mutex_);
}

void UIManager::setInCO2(uint16_t ppm) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (ppm == 0) {
        lv_label_set_text(ui_co2_value, "--");
    } else {
        lv_label_set_text_fmt(ui_co2_value, "%u", ppm);
    }

    xSemaphoreGive(mutex_);
}

void UIManager::setHVACState(ControllerDomain::HVACState state) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (state == HVACState::Off) {
        lv_obj_set_style_border_side(ui_Indoor_temp_value, LV_BORDER_SIDE_NONE, 0);
    } else {
        lv_obj_set_style_border_side(ui_Indoor_temp_value, LV_BORDER_SIDE_FULL, 0);
        if (state == HVACState::Heat) {
            lv_obj_set_style_border_color(ui_Indoor_temp_value, lv_color_hex(0xFF9040), 0);
        } else {
            lv_obj_set_style_border_color(ui_Indoor_temp_value, lv_color_hex(0x4CAFFF), 0);
        }
    }

    xSemaphoreGive(mutex_);
}

void UIManager::setCurrentSetpoints(double heatC, double coolC) {
    currHeatDeg_ = std::round(ABS_C_TO_F(heatC));
    currCoolDeg_ = std::round(ABS_C_TO_F(coolC));

    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_label_set_text_fmt(ui_Heat_setpoint, "%u", currHeatDeg_);
    lv_label_set_text_fmt(ui_Cool_setpoint, "%u", currCoolDeg_);
    xSemaphoreGive(mutex_);
}

void UIManager::setSystemPower(bool on) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_obj_t *objs[] = {
        ui_Temp_setpoint_container,
        ui_co2_setpoint_container,
    };

    lv_opa_t opa = on ? LV_OPA_100 : LV_OPA_50;

    for (int i = 0; i < (sizeof(objs) / sizeof(objs[0])); i++) {
        lv_obj_set_style_opa(objs[i], opa, 0);
    }

    objSetVisibility(!on, ui_on_button);
    objSetVisibility(on, ui_off_button);
    xSemaphoreGive(mutex_);
}
