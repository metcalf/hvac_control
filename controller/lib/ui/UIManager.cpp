#include "UIManager.h"

#include <cmath>

using HVACState = ControllerDomain::HVACState;
using Config = ControllerDomain::Config;

void UIManager::bootDone() {
    _ui_screen_change(&ui_Home, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_Home_screen_init);
}

void UIManager::bootErr(const char *msg) {
    lv_label_set_text(ui_boot_error_text, msg);

    lv_obj_add_flag(ui_boot_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_boot_error_heading, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_boot_error_text, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::sendACOverrideEvent(ACOverride override) {
    Event evt{EventType::ACOverride, EventPayload{.acOverride = override}};
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);
}

void UIManager::sendPowerEvent(bool on) {
    Event evt{EventType::SetSystemPower, EventPayload{.systemPower = on}};
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);
}

double UIManager::getHeatRollerValue(lv_obj_t *roller) {
    return ABS_F_TO_C(MIN_HEAT_F + lv_roller_get_selected(roller));
}
double UIManager::getCoolRollerValue(lv_obj_t *roller) {
    return ABS_F_TO_C(minCoolF_ + lv_roller_get_selected(roller));
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
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);
}

void UIManager::eThermotatOverride() {
    Event evt{EventType::TempOverride,
              EventPayload{.tempOverride = {
                               .heatC = getHeatRollerValue(ui_Heat_override_setpoint),
                               .coolC = getCoolRollerValue(ui_Cool_override_setpoint),
                           }}};
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);
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
    uint16_t target = 800 + lv_roller_get_selected(ui_co2_target) * 50;
    Event evt{EventType::SetCO2Target, EventPayload{.co2Target = target}};
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);

    lv_label_set_text_fmt(ui_co2_target_value, "%u", target);
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
                                   .startHr = (uint8_t)lv_roller_get_selected(ui_Night_hr),
                                   .startMin = (uint8_t)lv_roller_get_selected(ui_Night_min),
                               },
                           }}};
    xQueueSend(eventQueue_, &evt, portMAX_DELAY);
}

UIManager::UIManager(ControllerDomain::Config config) {
    minCoolF_ = ABS_C_TO_F(config.minCoolC) + 0.5;

    // Populate schedule values
    // Populate rollers based on mix/max heat/cool

    lv_label_set_text_fmt(ui_co2_target_value, "%u", config.co2Target);

    // Hide controls/values for HVAC modes we don't support
    if (config.heatType == Config::HVACType::None) {
        lv_obj_add_flag(ui_Heat_setpoint_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Override_heat_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Day_heat_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Night_heat_container, LV_OBJ_FLAG_HIDDEN);
    }
    if (config.coolType == Config::HVACType::None) {
        lv_obj_add_flag(ui_Cool_setpoint_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Override_cool_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Day_cool_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Night_cool_container, LV_OBJ_FLAG_HIDDEN);
    }

    // TODO: FUTURE hide buttons/controls on secondary controller
}

void UIManager::setHumidity(double h) {
    lv_label_set_text_fmt(ui_Humidity_value, "%u", std::min(99U, (uint)(h + 0.5)));
}

void UIManager::setCurrentFanSpeed(uint8_t speed) {
    if (speed == 0) {
        lv_label_set_text(ui_Fan_value, "OFF");
    } else {
        lv_label_set_text_fmt(ui_Fan_value, "%u%%", (uint)(speed / 255.0 * 100 + 0.5));
    }
}

void UIManager::setOutTempC(double tc) {
    lv_label_set_text_fmt(ui_Out_temp_value, "%u°", (uint)(ABS_C_TO_F(tc) + 0.5));
}

void UIManager::setInTempC(double tc) {
    lv_label_set_text_fmt(ui_Indoor_temp_value, "%u°", (uint)(ABS_C_TO_F(tc) + 0.5));
}

void UIManager::setInCO2(uint16_t ppm) { lv_label_set_text_fmt(ui_co2_value, "%u", ppm); }

void UIManager::setHVACState(ControllerDomain::HVACState state) {
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
}

void UIManager::setCurrentSetpoints(double heatC, double coolC) {
    lv_label_set_text_fmt(ui_Heat_setpoint, "%u", (uint)(ABS_C_TO_F(heatC) + 0.5));
    lv_label_set_text_fmt(ui_Cool_setpoint, "%u", (uint)(ABS_C_TO_F(coolC) + 0.5));
}

void UIManager::setSystemPower(bool on) {
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
}

// TODO: some way to set messages like the following
// Maybe scroll through them?
// https://forum.lvgl.io/t/how-to-scroll-items-circularly/9504
// Need some way to automatically clear messages too
// (X) Fan @ 66% for 4:23
// (X) Temp override until XX:XXXM
// ERR: ...
// Precooling for evening (fan / A/C + fan)
// Waiting for outdoor temp to drop to cool
// Replace filter
// Ventilating
// Fetching time
// Wifi connection failed
// (X) Forcing A/C cooling until XX:XXXM
// (X) A/C disabled until XX:XXXM
void UIManager::setMessage(uint8_t msgID, bool allowCancel, const char *msg) {}

void UIManager::clearMessage(uint8_t msgID) {}
