// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: ui

#include "../ui.h"

void ui_Boot_screen_init(void)
{
ui_Boot = lv_obj_create(NULL);
lv_obj_clear_flag( ui_Boot, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_boot_message = lv_label_create(ui_Boot);
lv_obj_set_width( ui_boot_message, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_boot_message, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_boot_message, LV_ALIGN_CENTER );
lv_label_set_text(ui_boot_message,"HELLO");
lv_obj_set_style_text_color(ui_boot_message, lv_color_hex(0xE0E0E0), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_boot_message, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_boot_message, &lv_font_montserrat_48, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_boot_error_heading = lv_label_create(ui_Boot);
lv_obj_set_width( ui_boot_error_heading, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_boot_error_heading, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_boot_error_heading, LV_ALIGN_TOP_MID );
lv_label_set_text(ui_boot_error_heading,"STARTUP ERROR");
lv_obj_add_flag( ui_boot_error_heading, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_set_style_text_color(ui_boot_error_heading, lv_color_hex(0xFF6868), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_boot_error_heading, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_boot_error_heading, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_boot_error_heading, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_boot_error_heading, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_boot_error_heading, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_boot_error_heading, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Container6 = lv_obj_create(ui_Boot);
lv_obj_remove_style_all(ui_Container6);
lv_obj_set_height( ui_Container6, 188);
lv_obj_set_width( ui_Container6, lv_pct(100));
lv_obj_set_align( ui_Container6, LV_ALIGN_BOTTOM_MID );
lv_obj_clear_flag( ui_Container6, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_boot_error_text = lv_label_create(ui_Container6);
lv_obj_set_width( ui_boot_error_text, lv_pct(100));
lv_obj_set_height( ui_boot_error_text, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_boot_error_text, LV_ALIGN_TOP_MID );
lv_obj_add_flag( ui_boot_error_text, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_set_style_text_align(ui_boot_error_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_boot_error_text, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_boot_error_text, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_boot_error_text, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_boot_error_text, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_boot_error_text, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

}