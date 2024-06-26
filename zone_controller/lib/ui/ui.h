// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.0
// LVGL version: 8.3.11
// Project name: zone_controller_ui

#ifndef _ZONE_CONTROLLER_UI_UI_H
#define _ZONE_CONTROLLER_UI_UI_H

#ifdef __cplusplus
extern "C" {
#endif

    #include "lvgl/lvgl.h"

#include "ui_helpers.h"
#include "ui_events.h"
#include "ui_theme_manager.h"
#include "ui_themes.h"

// SCREEN: ui_Home
void ui_Home_screen_init(void);
extern lv_obj_t *ui_Home;
extern lv_obj_t *ui_test_mode_header;
extern lv_obj_t *ui_test_mode_header_label;
extern lv_obj_t *ui_normal_mode_header;
extern lv_obj_t *ui_heat_pump_symbol;
extern lv_obj_t *ui_heat_pump_state;
void ui_event_Label6( lv_event_t * e);
extern lv_obj_t *ui_Label6;
extern lv_obj_t *ui_zones_container;
extern lv_obj_t *ui_Label1;
extern lv_obj_t *ui_zone_container_1;
extern lv_obj_t *ui_zone_label_1;
extern lv_obj_t *ui_tstat_state_1;
extern lv_obj_t *ui_vlv_state_1;
extern lv_obj_t *ui_zone_container_2;
extern lv_obj_t *ui_zone_label_2;
extern lv_obj_t *ui_tstat_state_2;
extern lv_obj_t *ui_vlv_state_2;
extern lv_obj_t *ui_zone_container_3;
extern lv_obj_t *ui_zone_label_3;
extern lv_obj_t *ui_tstat_state_3;
extern lv_obj_t *ui_vlv_state_3;
extern lv_obj_t *ui_zone_container_4;
extern lv_obj_t *ui_zone_label_4;
extern lv_obj_t *ui_tstat_state_4;
extern lv_obj_t *ui_vlv_state_4;
extern lv_obj_t *ui_zone_pump_container;
extern lv_obj_t *ui_zone_pump_label;
extern lv_obj_t *ui_zone_pump_state;
extern lv_obj_t *ui_fancoils_container;
extern lv_obj_t *ui_Label2;
extern lv_obj_t *ui_fc_container_1;
extern lv_obj_t *ui_fc_label_1;
extern lv_obj_t *ui_fc_state_1;
extern lv_obj_t *ui_fc_container_2;
extern lv_obj_t *ui_fc_label_2;
extern lv_obj_t *ui_fc_state_2;
extern lv_obj_t *ui_fc_container_3;
extern lv_obj_t *ui_fc_label_3;
extern lv_obj_t *ui_fc_state_3;
extern lv_obj_t *ui_fc_container_4;
extern lv_obj_t *ui_fc_label_4;
extern lv_obj_t *ui_fc_state_4;
extern lv_obj_t *ui_fc_pump_container;
extern lv_obj_t *ui_zone_label_8;
extern lv_obj_t *ui_fc_pump_state;
extern lv_obj_t *ui_normal_mode_footer;
extern lv_obj_t *ui_test_mode_footer;
void ui_event_exit_test_mode_button( lv_event_t * e);
extern lv_obj_t *ui_exit_test_mode_button;
extern lv_obj_t *ui_Label7;
void ui_event_control_overlay( lv_event_t * e);
extern lv_obj_t *ui_control_overlay;
extern lv_obj_t *ui_Container9;
extern lv_obj_t *ui_test_mode_button;
extern lv_obj_t *ui_Label3;
extern lv_obj_t *ui_system_power_container;
extern lv_obj_t *ui_system_off_button;
extern lv_obj_t *ui_Label4;
extern lv_obj_t *ui_system_on_button;
extern lv_obj_t *ui_Label5;
extern lv_obj_t *ui_end_lockout_button;
extern lv_obj_t *ui_end_lockout_label;
extern lv_obj_t *ui____initial_actions0;



LV_FONT_DECLARE( ui_font_MaterialSymbols18);
LV_FONT_DECLARE( ui_font_MaterialSymbols24);
LV_FONT_DECLARE( ui_font_MaterialSymbols36);


void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
