// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ui

#include "../ui.h"

void ui_CO2_screen_init(void)
{
ui_CO2 = lv_obj_create(NULL);
lv_obj_clear_flag( ui_CO2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_flex_flow(ui_CO2,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_CO2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_obj_set_style_pad_left(ui_CO2, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_CO2, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_CO2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_CO2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_row(ui_CO2, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_column(ui_CO2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Thermostat_setting_header1 = ui_Setting_header_create(ui_CO2);
lv_obj_set_x( ui_Thermostat_setting_header1, 0 );
lv_obj_set_y( ui_Thermostat_setting_header1, 0 );

lv_label_set_text(ui_comp_get_child(ui_Thermostat_setting_header1, UI_COMP_SETTING_HEADER_SETTING_TITLE),"CO2 / VENTILATION");





ui_Container2 = lv_obj_create(ui_CO2);
lv_obj_remove_style_all(ui_Container2);
lv_obj_set_width( ui_Container2, 300);
lv_obj_set_height( ui_Container2, 190);
lv_obj_set_x( ui_Container2, 0 );
lv_obj_set_y( ui_Container2, -7 );
lv_obj_set_align( ui_Container2, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_Container2,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_Container2, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
lv_obj_clear_flag( ui_Container2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Override_speed_container1 = lv_obj_create(ui_Container2);
lv_obj_remove_style_all(ui_Override_speed_container1);
lv_obj_set_height( ui_Override_speed_container1, 143);
lv_obj_set_width( ui_Override_speed_container1, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Override_speed_container1, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Override_speed_container1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_co2_target = lv_roller_create(ui_Override_speed_container1);
lv_roller_set_options( ui_co2_target, "OFF\n800\n850\n900\n950\n1000\n1050\n1100\n1150\n1200\n1250\n1300\n1350\n1400\n1450\n1500\n1550\n1600\n1650\n1700\n1750\n1800\n1850\n1900\n1950\n2000", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_co2_target, 100);
lv_obj_set_width( ui_co2_target, LV_SIZE_CONTENT);  /// 60
lv_obj_set_align( ui_co2_target, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_co2_target, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_co2_target, lv_color_hex(0x44474C), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_co2_target, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Container11 = lv_obj_create(ui_Override_speed_container1);
lv_obj_remove_style_all(ui_Container11);
lv_obj_set_width( ui_Container11, LV_SIZE_CONTENT);  /// 100
lv_obj_set_height( ui_Container11, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_Container11, LV_ALIGN_BOTTOM_MID );
lv_obj_set_flex_flow(ui_Container11,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_Container11, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
lv_obj_clear_flag( ui_Container11, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_co2_symbol1 = lv_label_create(ui_Container11);
lv_obj_set_width( ui_co2_symbol1, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_co2_symbol1, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_co2_symbol1, 0 );
lv_obj_set_y( ui_co2_symbol1, 1 );
lv_obj_set_align( ui_co2_symbol1, LV_ALIGN_LEFT_MID );
lv_label_set_text(ui_co2_symbol1,"");
lv_obj_set_style_text_color(ui_co2_symbol1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_co2_symbol1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_co2_symbol1, &ui_font_MaterialSymbols18, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_co2_symbol1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_co2_symbol1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_co2_symbol1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_co2_symbol1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Label11 = lv_label_create(ui_Container11);
lv_obj_set_width( ui_Label11, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label11, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_Label11, 0 );
lv_obj_set_y( ui_Label11, -2 );
lv_obj_set_align( ui_Label11, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label11,"PPM");
lv_obj_set_style_text_color(ui_Label11, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label11, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label11, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_add_event_cb(ui_comp_get_child(ui_Thermostat_setting_header1, UI_COMP_SETTING_HEADER_SETTING_BACK_BUTTON_SETTING_BACK_LABEL), ui_event_Thermostat_setting_header1_Setting_back_button_Setting_back_label, LV_EVENT_ALL, NULL);
lv_obj_add_event_cb(ui_comp_get_child(ui_Thermostat_setting_header1, UI_COMP_SETTING_HEADER_SETTING_ACCEPT_BUTTON_SETTING_ACCEPT_LABEL), ui_event_Thermostat_setting_header1_Setting_accept_button_Setting_accept_label, LV_EVENT_ALL, NULL);

}