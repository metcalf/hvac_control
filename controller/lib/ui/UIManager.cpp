#include "UIManager.h"

#include "esp_log.h"
#include <cmath>

static const char *TAG = "UI";

using HVACState = ControllerDomain::HVACState;
using Config = ControllerDomain::Config;

#define MSG_SCROLL_MS 5 * 1000

void messageTimerCb(lv_timer_t *timer) { ((UIManager *)timer->user_data)->onMessageTimer(); }

void pauseTimer(lv_event_t *e) { lv_timer_pause((lv_timer_t *)e->user_data); }

void resetAndResumeTimer(lv_event_t *e) {
    lv_timer_reset((lv_timer_t *)e->user_data);
    lv_timer_resume((lv_timer_t *)e->user_data);
}

void UIManager::bootDone() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    _ui_screen_change(&ui_Home, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Home_screen_init);
    xSemaphoreGive(mutex_);
}

void UIManager::bootErr(const char *msg) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_label_set_text(ui_boot_error_text, msg);

    lv_obj_add_flag(ui_boot_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_boot_error_heading, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_boot_error_text, LV_OBJ_FLAG_HIDDEN);
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

double UIManager::getHeatRollerValue(lv_obj_t *roller) {
    return ABS_F_TO_C(minHeatDeg_ + lv_roller_get_selected(roller));
}
double UIManager::getCoolRollerValue(lv_obj_t *roller) {
    return ABS_F_TO_C(minCoolDeg_ + lv_roller_get_selected(roller));
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
                               .heatC = getHeatRollerValue(ui_Heat_override_setpoint),
                               .coolC = getCoolRollerValue(ui_Cool_override_setpoint),
                           }}};
    eventCb_(evt);
}

void UIManager::eUseAC() {
    sendACOverrideEvent(ACOverride::Force);

    // Hide "use A/C" and show "stop A/C"
    lv_obj_add_flag(ui_use_ac_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_stop_ac_button, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::eStopAC() {
    sendACOverrideEvent(ACOverride::Stop);

    // Hide "stop A/C" and show "allow A/C"
    lv_obj_add_flag(ui_stop_ac_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_allow_ac_button, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::eAllowAC() {
    sendACOverrideEvent(ACOverride::Normal);

    // Hide "allow A/C" and show "stop A/C"
    lv_obj_add_flag(ui_allow_ac_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_stop_ac_button, LV_OBJ_FLAG_HIDDEN);
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
    Event evt{EventType::SetSchedule,
              EventPayload{.schedules = {
                               {
                                   .heatC = getHeatRollerValue(ui_Day_heat_setpoint),
                                   .coolC = getHeatRollerValue(ui_Day_cool_setpoint),
                                   .startHr = (uint8_t)lv_roller_get_selected(ui_Day_hr),
                                   .startMin = (uint8_t)lv_roller_get_selected(ui_Day_min),
                               },
                               {
                                   .heatC = getHeatRollerValue(ui_Night_heat_setpoint),
                                   .coolC = getHeatRollerValue(ui_Night_cool_setpoint),
                                   .startHr = (uint8_t)(lv_roller_get_selected(ui_Night_hr) + 12),
                                   .startMin = (uint8_t)lv_roller_get_selected(ui_Night_min),
                               },
                           }}};

    currSchedules_[0] = evt.payload.schedules[0];
    currSchedules_[1] = evt.payload.schedules[1];

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
    lv_roller_set_selected(ui_Heat_override_setpoint, currHeatDeg_ - minHeatDeg_, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Cool_override_setpoint, currCoolDeg_ - minCoolDeg_, LV_ANIM_OFF);
}

void UIManager::eScheduleLoadStart() {
    ESP_LOGD(TAG, "scheduleLoadStart");
    lv_roller_set_selected(ui_Day_heat_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[0].heatC)) - minHeatDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_cool_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[0].coolC)) - minCoolDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_hr, currSchedules_[0].startHr, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Day_min, currSchedules_[0].startMin, LV_ANIM_OFF);

    lv_roller_set_selected(ui_Night_heat_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[1].heatC)) - minHeatDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_cool_setpoint,
                           std::round(ABS_C_TO_F(currSchedules_[1].coolC)) - minCoolDeg_,
                           LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_hr, currSchedules_[1].startHr - 12, LV_ANIM_OFF);
    lv_roller_set_selected(ui_Night_min, currSchedules_[1].startMin, LV_ANIM_OFF);
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

UIManager::UIManager(ControllerDomain::Config config, size_t nMsgIds, eventCb_t eventCb)
    : eventCb_(eventCb) {
    mutex_ = xSemaphoreCreateMutex();

    co2Target_ = config.co2Target;

    ui_init();

    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        currSchedules_[i] = config.schedules[i];
    }
    maxHeatDeg_ = std::round(ABS_C_TO_F(config.maxHeatC));
    minCoolDeg_ = std::round(ABS_C_TO_F(config.minCoolC));

    setupTempRoller(ui_Heat_override_setpoint, minHeatDeg_, maxHeatDeg_);
    setupTempRoller(ui_Cool_override_setpoint, minCoolDeg_, maxCoolDeg_);
    setupTempRoller(ui_Day_heat_setpoint, minHeatDeg_, maxHeatDeg_);
    setupTempRoller(ui_Day_cool_setpoint, minCoolDeg_, maxCoolDeg_);
    setupTempRoller(ui_Night_heat_setpoint, minHeatDeg_, maxHeatDeg_);
    setupTempRoller(ui_Night_cool_setpoint, minCoolDeg_, maxCoolDeg_);

    lv_label_set_text_fmt(ui_co2_target_value, "%u", config.co2Target);

    // Hide AC buttons if we don't have active cooling
    if (config.coolType == Config::HVACType::None) {
        lv_obj_add_flag(ui_ac_button_container, LV_OBJ_FLAG_HIDDEN);
    }

    // Disable some controls on the secondary controller to simplify communications
    if (config.controllerType == Config::ControllerType::Secondary) {
        lv_obj_add_flag(ui_ac_button_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_power_button_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_fan_override_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            ui_comp_get_child(ui_fan_setting_header,
                              UI_COMP_SETTING_HEADER_SETTING_ACCEPT_BUTTON_SETTING_ACCEPT_LABEL),
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_fan_override_secondary_container, LV_OBJ_FLAG_HIDDEN);
    }

    // Delete the message container we created with Squareline for design purposes
    lv_obj_del(ui_message_container);

    nMsgIds_ = nMsgIds;
    messages_ = new MessageContainer *[nMsgIds];

    for (int i = 0; i < nMsgIds; i++) {
        messages_[i] = new MessageContainer(ui_Footer);
    }

    msgHeight_ = messages_[0]->getHeight();
    msgTimer_ = lv_timer_create(messageTimerCb, MSG_SCROLL_MS, this);

    lv_obj_add_event_cb(ui_Footer, pauseTimer, LV_EVENT_SCROLL_BEGIN, msgTimer_);
    lv_obj_add_event_cb(ui_Footer, resetAndResumeTimer, LV_EVENT_SCROLL_END, msgTimer_);
}

void UIManager::setHumidity(double h) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_label_set_text_fmt(ui_Humidity_value, "%u%%", std::min(99U, (uint)std::round(h)));
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
    if (std::isnan(tc)) {
        lv_label_set_text(ui_Out_temp_value, "--");
    } else {
        lv_label_set_text_fmt(ui_Out_temp_value, "%u°", (uint)std::round(ABS_C_TO_F(tc)));
    }
    xSemaphoreGive(mutex_);
}

void UIManager::setInTempC(double tc) {
    lv_label_set_text_fmt(ui_Indoor_temp_value, "%u°", (uint)std::round(ABS_C_TO_F(tc)));
}

void UIManager::setInCO2(uint16_t ppm) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lv_label_set_text_fmt(ui_co2_value, "%u", ppm);
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
        lv_obj_add_flag(ui_use_ac_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_stop_ac_button, LV_OBJ_FLAG_HIDDEN);
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

    lv_obj_add_flag(on ? ui_on_button : ui_off_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(on ? ui_off_button : ui_on_button, LV_OBJ_FLAG_HIDDEN);
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

    if (visible) {
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }

    visible_ = visible;
}

void UIManager::MessageContainer::setCancelable(bool cancelable) {
    if (cancelable_ == cancelable) {
        return;
    }

    if (cancelable) {
        lv_obj_clear_flag(cancel_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(cancel_, LV_OBJ_FLAG_HIDDEN);
    }

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
