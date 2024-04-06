#include "ZCUIManager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "ui_utils.h"

#include <functional>

lv_obj_t **zoneContainers[] = {
    &ui_zone_container_1,
    &ui_zone_container_2,
    &ui_zone_container_3,
    &ui_zone_container_4,
};
lv_obj_t **zoneCalls[] = {
    &ui_tstat_state_1,
    &ui_tstat_state_2,
    &ui_tstat_state_3,
    &ui_tstat_state_4,
};
lv_obj_t **zoneValves[] = {
    &ui_vlv_state_1,
    &ui_vlv_state_2,
    &ui_vlv_state_3,
    &ui_vlv_state_4,
};
lv_obj_t **fcContainers[] = {
    &ui_fc_container_1,
    &ui_fc_container_2,
    &ui_fc_container_3,
    &ui_fc_container_4,
};
lv_obj_t **fcCalls[] = {
    &ui_fc_state_1,
    &ui_fc_state_2,
    &ui_fc_state_3,
    &ui_fc_state_4,
};

typedef std::function<void(lv_event_t *)> uiCbFn;

void uiEventCb(lv_event_t *e) { (*(uiCbFn *)lv_event_get_user_data(e))(e); }

static const char *TAG = "UI";

static const char *HEAT_CHAR = "\uf537";
static const char *COOL_CHAR = "\uf166";
static const char *OFF_CHAR = "\ue836";

ZCUIManager::ZCUIManager(SystemState state, size_t nMsgIds, eventCb_t eventCb) : eventCb_(eventCb) {
    mutex_ = xSemaphoreCreateMutex();
    ui_init();

    sleepMgr_ = new SleepManager(ui_Home, GPIO_NUM_48);
    msgMgr_ = new MessageManager(nMsgIds, ui_normal_mode_footer, &ui_font_MaterialSymbols24, NULL);

    lv_obj_add_event_cb(ui_system_on_button, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onSystemPower(true); }));
    lv_obj_add_event_cb(ui_system_off_button, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onSystemPower(false); }));
    lv_obj_add_event_cb(ui_test_mode_button, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onTestMode(true); }));
    lv_obj_add_event_cb(ui_exit_test_mode_button, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onTestMode(false); }));
    lv_obj_add_event_cb(ui_end_lockout_button, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onEndLockout(); }));

    // Configure test mode events for each zone/fancoil/pump
    for (int i = 0; i < 4; i++) {
        lv_obj_add_event_cb(*zoneContainers[i], uiEventCb, LV_EVENT_CLICKED,
                            new uiCbFn([this, i](lv_event_t *e) { onZoneToggle(i); }));
    }
    lv_obj_add_event_cb(ui_zone_pump_container, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onPumpToggle(Pump::Zone); }));
    lv_obj_add_event_cb(ui_fc_pump_container, uiEventCb, LV_EVENT_CLICKED,
                        new uiCbFn([this](lv_event_t *e) { onPumpToggle(Pump::Fancoil); }));

    updateState(state);
}

uint32_t ZCUIManager::handleTasks() {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    sleepMgr_->update();
    auto rv = lv_timer_handler();
    xSemaphoreGive(mutex_);
    return rv;
}

void ZCUIManager::updateState(SystemState state) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (int i = 0; i < 4; i++) {
        updateCall(*zoneCalls[i], state.thermostats[i]);
        lv_label_set_text(*zoneValves[i], getValveText(state.valves[i]));
        updateCall(*fcCalls[i], state.fancoils[i]);
    }
    updatePump(ui_zone_pump_state, state.zonePump);
    updatePump(ui_fc_pump_state, state.fcPump);

    lv_label_set_text(ui_heat_pump_state, getHeatPumpText(state.heatPumpMode));
    xSemaphoreGive(mutex_);
}

void ZCUIManager::updateCall(lv_obj_t *icon, Call call) {
    const ui_theme_variable_t *color = NULL;
    const char *txt = NULL;

    switch (call) {
    case Call::None:
        color = _ui_theme_color_border;
        txt = OFF_CHAR;
        break;
    case Call::Heat:
        color = _ui_theme_color_heat;
        txt = HEAT_CHAR;
        break;
    case Call::Cool:
        color = _ui_theme_color_cool;
        txt = COOL_CHAR;
        break;
    }

    lv_label_set_text_static(icon, txt);
    ui_object_set_themeable_style_property(icon, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_TEXT_COLOR, color);
}

void ZCUIManager::updatePump(lv_obj_t *pmp, bool on) { lv_label_set_text(pmp, on ? "ON" : "OFF"); }

const char *ZCUIManager::getValveText(ValveState state) {
    if (state.action == ValveAction::Wait) {
        return "WAITNG";
    } else if (state.target) {
        if (state.action == ValveAction::Set) {
            return "OPEN";
        } else {
            return "OPENING";
        }
    } else {
        if (state.action == ValveAction::Set) {
            return "CLOSED";
        } else {
            return "CLOSING";
        }
    }
}

const char *ZCUIManager::getHeatPumpText(HeatPumpMode state) {
    switch (state) {
    case HeatPumpMode::Off:
        return "OFF";
    case HeatPumpMode::Heat:
        return "HEAT";
    case HeatPumpMode::Cool:
        return "COOL";
    case HeatPumpMode::Standby:
        return "STANDBY";
    }

    return nullptr;
}

void ZCUIManager::onSystemPower(bool on) {
    objSetVisibility(on, ui_system_off_button);
    objSetVisibility(!on, ui_system_on_button);

    Event evt{EventType::SetSystemPower, EventPayload{.systemPower = on}};
    eventCb_(evt);
}

void ZCUIManager::onTestMode(bool on) {
    for (int i = 0; i < 4; i++) {
        objSetFlag(on, *zoneContainers[i], LV_OBJ_FLAG_CLICKABLE);
    }
    objSetFlag(on, ui_zone_pump_container, LV_OBJ_FLAG_CLICKABLE);
    objSetFlag(on, ui_fc_pump_container, LV_OBJ_FLAG_CLICKABLE);

    if (on) {
        objSetVisibility(false, ui_control_overlay);
    }
    objSetVisibility(!on, ui_normal_mode_header);
    objSetVisibility(on, ui_test_mode_header);
    objSetVisibility(!on, ui_normal_mode_footer);
    objSetVisibility(on, ui_test_mode_footer);

    Event evt{EventType::SetTestMode, EventPayload{.testMode = on}};
    eventCb_(evt);
}

void ZCUIManager::onEndLockout() {
    objSetVisibility(false, ui_control_overlay);

    Event evt{EventType::ResetHVACLockout, EventPayload{}};
    eventCb_(evt);
}

void ZCUIManager::onZoneToggle(uint8_t i) {
    Event evt{EventType::TestToggleZone, EventPayload{.zone = i}};
    eventCb_(evt);
}

void ZCUIManager::onPumpToggle(Pump pump) {
    Event evt{EventType::TestTogglePump, EventPayload{.pump = pump}};
    eventCb_(evt);
}
