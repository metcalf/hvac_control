#include "UIManager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include <cmath>

using HVACState = ControllerDomain::HVACState;
using Config = ControllerDomain::Config;

#define RESTART_DELAY_MS 1500
#define MSG_SCROLL_MS 5 * 1000
#define DISP_SLEEP_SECS 30
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

void messageTimerCb(lv_timer_t *timer) { ((UIManager *)timer->user_data)->onMessageTimer(); }

void restartCb(lv_timer_t *) { esp_restart(); }

void pauseTimer(lv_event_t *e) { lv_timer_pause((lv_timer_t *)e->user_data); }

void resetAndResumeTimer(lv_event_t *e) {
    lv_timer_reset((lv_timer_t *)e->user_data);
    lv_timer_resume((lv_timer_t *)e->user_data);
}

void wifiTextareaEventCb(lv_event_t *e) {
    ((UIManager *)lv_event_get_user_data(e))->eWifiTextarea(e);
}

void heatRollerChangeCb(lv_event_t *e) {
    UIManager::eventsInst()->handleTempRollerChange(true, lv_event_get_target(e),
                                                    (lv_obj_t *)lv_event_get_user_data(e));
}

void coolRollerChangeCb(lv_event_t *e) {
    UIManager::eventsInst()->handleTempRollerChange(true, (lv_obj_t *)lv_event_get_user_data(e),
                                                    lv_event_get_target(e));
}

void objSetFlag(bool on, lv_obj_t *obj, lv_obj_flag_t f) {
    if (on) {
        lv_obj_add_flag(obj, f);
    } else {
        lv_obj_clear_flag(obj, f);
    }
}

void objSetVisibility(bool visible, lv_obj_t *obj) {
    objSetFlag(!visible, obj, LV_OBJ_FLAG_HIDDEN);
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

UIManager::MessageContainer *UIManager::focusedMessage() {
    for (int i = 0; i < nMsgIds_; i++) {
        if (messages_[i]->isFocused()) {
            return messages_[i];
        }
    }

    return nullptr;
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

void UIManager::eUseAC() {
    sendACOverrideEvent(ACOverride::Force);

    // Hide "use A/C" and show "stop A/C"
    objSetVisibility(false, ui_use_ac_button);
    objSetVisibility(true, ui_stop_ac_button);
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
    lv_timer_reset(msgTimer_);
    lv_timer_resume(msgTimer_);
}

void UIManager::eHomeUnloadStart() {
    ESP_LOGD(TAG, "homeUnloadStart");
    lv_timer_pause(msgTimer_);
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
    Config::ControllerType newType =
        Config::ControllerType(lv_dropdown_get_selected(ui_controller_type));
    bool typeChanged = newType != equipment_.controllerType;

    equipment_.controllerType = newType;
    equipment_.heatType = Config::HVACType(lv_dropdown_get_selected(ui_heat_type));
    equipment_.coolType = Config::HVACType(lv_dropdown_get_selected(ui_cool_type));
    equipment_.hasMakeupDemand = lv_obj_has_state(ui_makeup_air_switch, LV_STATE_CHECKED);

    updateUIForEquipment();

    Event evt{
        EventType::SetEquipment,
        EventPayload{.equipment = equipment_},
    };

    eventCb_(evt);

    // Show reboot screen if the controller changed, otherwise back to settings
    if (typeChanged) {
        lv_timer_resume(restartTimer_);
        _ui_screen_change(&ui_Restart, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Restart_screen_init);
    } else {
        _ui_screen_change(&ui_Settings, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Settings_screen_init);
    }
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
    lv_dropdown_set_selected(ui_controller_type, static_cast<uint16_t>(equipment_.controllerType));
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

void UIManager::onMessageTimer() {
    // TODO(future): It'd be nicer to make this a circular scroll
    lv_coord_t currY = lv_obj_get_scroll_y(ui_Footer);

    int nVisible = 0;
    for (int i = 0; i < nMsgIds_; i++) {
        if (messages_[i]->isVisible()) {
            nVisible++;
        }
    }

    lv_coord_t newY = currY + msgHeight_;
    if (newY > (nVisible - 1) * msgHeight_) {
        newY = 0;
    }

    lv_obj_scroll_to_y(ui_Footer, newY, LV_ANIM_ON);
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

    bool isSecondary = equipment_.controllerType == Config::ControllerType::Secondary;

    // Disable some controls on the secondary controller to simplify communications
    objSetVisibility(!isSecondary, ui_ac_button_container);
    objSetVisibility(!isSecondary, ui_power_button_container);
    objSetVisibility(!isSecondary, ui_fan_override_container);
    objSetVisibility(
        !isSecondary,
        ui_comp_get_child(ui_fan_setting_header,
                          UI_COMP_SETTING_HEADER_SETTING_ACCEPT_BUTTON_SETTING_ACCEPT_LABEL));
    objSetVisibility(isSecondary, ui_fan_override_secondary_container);

    // Settings
    objSetVisibility(!isSecondary, ui_heat_type_container);
    objSetVisibility(!isSecondary, ui_cool_type_container);
    objSetVisibility(!isSecondary, ui_makeup_air_container);
    objSetVisibility(!isSecondary, ui_fan_offset_container);
    objSetVisibility(!isSecondary, ui_fan_offset_divider);
}

void UIManager::manageSleep() {
    if (!booted_) {
        if (!displayAwake_) {
            disp_backlight_set(backlight_, 100);
            displayAwake_ = true;
        }
        // Don't sleep on boot screen
        return;
    }

    if (lv_disp_get_inactive_time(NULL) < (DISP_SLEEP_SECS * 1000)) {
        if (!displayAwake_) {
            // Go to homescreen
            lv_indev_wait_release(lv_indev_get_act());
            _ui_screen_change(&ui_Home, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Home_screen_init);
            disp_backlight_set(backlight_, 100);
            displayAwake_ = true;
        }
    } else {
        if (displayAwake_) {
            disp_backlight_set(backlight_, 0);
            lv_scr_load_anim(sleepScreen_, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
            displayAwake_ = false;
        }
    }
}

UIManager::UIManager(ControllerDomain::Config config, size_t nMsgIds, eventCb_t eventCb)
    : eventCb_(eventCb) {
    mutex_ = xSemaphoreCreateMutex();

    inTempOffsetC_ = config.inTempOffsetC;
    outTempOffsetC_ = config.outTempOffsetC;
    co2Target_ = config.co2Target;
    equipment_ = config.equipment;
    wifi_ = config.wifi;

    const disp_backlight_config_t bckl_config = {
        .pwm_control = true,
        .output_invert = false, // Backlight on high
        .gpio_num = GPIO_NUM_48,
        .timer_idx = 0,
        .channel_idx = 0,
    };
    backlight_ = disp_backlight_new(&bckl_config);
    assert(backlight_);

    sleepScreen_ = lv_obj_create(NULL);
    ESP_LOGI(TAG, "created");
    ui_init();
    ESP_LOGI(TAG, "ui init");

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

    // Delete the message container we created with Squareline for design purposes
    lv_obj_del(ui_message_container);

    nMsgIds_ = nMsgIds;
    messages_ = new MessageContainer *[nMsgIds];

    for (int i = 0; i < nMsgIds; i++) {
        messages_[i] = new MessageContainer(ui_Footer);
    }

    lv_obj_set_scroll_snap_y(ui_Footer, LV_SCROLL_SNAP_START);
    msgHeight_ = messages_[0]->getHeight();
    msgTimer_ = lv_timer_create(messageTimerCb, MSG_SCROLL_MS, this);
    restartTimer_ = lv_timer_create(restartCb, RESTART_DELAY_MS, NULL);
    lv_timer_pause(restartTimer_);

    lv_obj_add_event_cb(ui_Footer, pauseTimer, LV_EVENT_SCROLL_BEGIN, msgTimer_);
    lv_obj_add_event_cb(ui_Footer, resetAndResumeTimer, LV_EVENT_SCROLL_END, msgTimer_);
}

uint32_t UIManager::handleTasks() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    manageSleep();
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

    if (state == HVACState::ACCool) {
        // Hide "use AC", show "stop A/C"
        objSetVisibility(false, ui_use_ac_button);
        objSetVisibility(true, ui_stop_ac_button);
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

void UIManager::setMessage(uint8_t msgID, bool allowCancel, const char *msg) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    MessageContainer *msgContainer = messages_[msgID];

    msgContainer->setText(msg);
    msgContainer->setCancelable(allowCancel);

    MessageContainer *focused = focusedMessage();
    if (focused == nullptr) {
        // Either there was no message or we're in the middle of scrolling
        // so just add to the end.
        msgContainer->setIndex(-1);
    } else if (msgContainer != focused) {
        msgContainer->setIndex(focused->getIndex() + 1);
    }

    msgContainer->setVisibility(true);
    xSemaphoreGive(mutex_);
}

void UIManager::clearMessage(uint8_t msgID) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    messages_[msgID]->setVisibility(false);
    xSemaphoreGive(mutex_);
}

UIManager::MessageContainer::MessageContainer(lv_obj_t *parent) {
    lv_obj_t *ui_message_container, *ui_message_close, *ui_message_text;
    lv_obj_t *ui_Footer = parent; // Gross but allows for copy-paste

    // BEGIN copy-paste from ui_Home.c
    ui_message_container = lv_obj_create(ui_Footer);
    lv_obj_remove_style_all(ui_message_container);
    lv_obj_set_width(ui_message_container, lv_pct(100));
    lv_obj_set_height(ui_message_container, lv_pct(100));
    lv_obj_set_align(ui_message_container, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_message_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_message_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_message_container, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_message_close = lv_label_create(ui_message_container);
    lv_obj_set_width(ui_message_close, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_message_close, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_message_close, LV_ALIGN_CENTER);
    lv_label_set_text(ui_message_close, "");
    lv_obj_set_style_text_font(ui_message_close, &ui_font_MaterialSymbols24,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_message_close, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_message_close, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_message_close, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_message_close, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_message_text = lv_label_create(ui_message_container);
    lv_obj_set_width(ui_message_text, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_message_text, LV_SIZE_CONTENT); /// 100
    lv_obj_set_align(ui_message_text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_message_text, "MESSAGE");
    // END copy-paste from ui_Home.c

    // NB: This is a bit awkard but allows directly copy-pasting from the Squareline generated code
    container_ = ui_message_container;
    cancel_ = ui_message_close;
    text_ = ui_message_text;
    parent_ = parent;

    setVisibility(false);
}

void UIManager::MessageContainer::setVisibility(bool visible) {
    if (visible_ == visible) {
        return;
    }

    objSetVisibility(visible, container_);
    visible_ = visible;
}

void UIManager::MessageContainer::setCancelable(bool cancelable) {
    if (cancelable_ == cancelable) {
        return;
    }

    objSetVisibility(cancelable, cancel_);

    cancelable_ = cancelable;
}

void UIManager::MessageContainer::setText(const char *str) {
    if (strncmp(str, lv_label_get_text(text_), UI_MAX_MSG_LEN) == 0) {
        // Strings are the same, don't cause an invalidation
        return;
    }

    lv_label_set_text(text_, str);
}

bool UIManager::MessageContainer::isFocused() {
    if (!visible_) {
        return false;
    }

    return lv_obj_get_scroll_y(parent_) == lv_obj_get_y(container_);
}
