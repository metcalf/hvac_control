// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: ui

#include "../ui.h"

void ui_Schedule_screen_init(void)
{
ui_Schedule = lv_obj_create(NULL);
lv_obj_clear_flag( ui_Schedule, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Schedule_header = ui_Setting_header_create(ui_Schedule);
lv_obj_set_x( ui_Schedule_header, 0 );
lv_obj_set_y( ui_Schedule_header, 0 );

lv_label_set_text(ui_comp_get_child(ui_Schedule_header, UI_COMP_SETTING_HEADER_SETTING_TITLE),"SCHEDULE");





ui_Container14 = lv_obj_create(ui_Schedule);
lv_obj_remove_style_all(ui_Container14);
lv_obj_set_width( ui_Container14, 320);
lv_obj_set_height( ui_Container14, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_Container14, LV_ALIGN_BOTTOM_MID );
lv_obj_clear_flag( ui_Container14, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_Container14, 15, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_Container14, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Container14, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_clip_corner(ui_Container14, true, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_TabView1 = lv_tabview_create(ui_Container14, LV_DIR_BOTTOM, 40);
lv_obj_set_width( ui_TabView1, 320);
lv_obj_set_height( ui_TabView1, 190);
lv_obj_set_align( ui_TabView1, LV_ALIGN_BOTTOM_MID );
lv_obj_clear_flag( ui_TabView1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

lv_obj_set_style_text_color(lv_tabview_get_tab_btns(ui_TabView1), lv_color_hex(0x808080),  LV_PART_ITEMS | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(lv_tabview_get_tab_btns(ui_TabView1), 255,  LV_PART_ITEMS| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(lv_tabview_get_tab_btns(ui_TabView1), lv_color_hex(0x808080),  LV_PART_ITEMS | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(lv_tabview_get_tab_btns(ui_TabView1), 255,  LV_PART_ITEMS| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(lv_tabview_get_tab_btns(ui_TabView1), 4,  LV_PART_ITEMS| LV_STATE_DEFAULT);
lv_obj_set_style_border_side(lv_tabview_get_tab_btns(ui_TabView1), LV_BORDER_SIDE_TOP,  LV_PART_ITEMS| LV_STATE_DEFAULT);
lv_obj_set_style_text_color(lv_tabview_get_tab_btns(ui_TabView1), lv_color_hex(0xFFFFFF),  LV_PART_ITEMS | LV_STATE_CHECKED );
lv_obj_set_style_text_opa(lv_tabview_get_tab_btns(ui_TabView1), 255,  LV_PART_ITEMS| LV_STATE_CHECKED);
lv_obj_set_style_bg_color(lv_tabview_get_tab_btns(ui_TabView1), lv_color_hex(0x808080),  LV_PART_ITEMS | LV_STATE_CHECKED );
lv_obj_set_style_bg_opa(lv_tabview_get_tab_btns(ui_TabView1), 255,  LV_PART_ITEMS| LV_STATE_CHECKED);
lv_obj_set_style_border_color(lv_tabview_get_tab_btns(ui_TabView1), lv_color_hex(0x808080),  LV_PART_ITEMS | LV_STATE_CHECKED );
lv_obj_set_style_border_opa(lv_tabview_get_tab_btns(ui_TabView1), 255,  LV_PART_ITEMS| LV_STATE_CHECKED);
lv_obj_set_style_border_side(lv_tabview_get_tab_btns(ui_TabView1), LV_BORDER_SIDE_TOP,  LV_PART_ITEMS| LV_STATE_CHECKED);

ui_DAY = lv_tabview_add_tab(ui_TabView1, "DAY");
lv_obj_set_flex_flow(ui_DAY,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_DAY, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_obj_set_style_pad_left(ui_DAY, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_DAY, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_DAY, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_DAY, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Day_heat_container = lv_obj_create(ui_DAY);
lv_obj_remove_style_all(ui_Day_heat_container);
lv_obj_set_height( ui_Day_heat_container, 140);
lv_obj_set_width( ui_Day_heat_container, LV_SIZE_CONTENT);  /// 2
lv_obj_set_align( ui_Day_heat_container, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Day_heat_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_blend_mode(ui_Day_heat_container, LV_BLEND_MODE_NORMAL, LV_PART_MAIN| LV_STATE_DISABLED);
lv_obj_set_style_opa(ui_Day_heat_container, 60, LV_PART_MAIN| LV_STATE_DISABLED);

ui_Day_heat_setpoint = lv_roller_create(ui_Day_heat_container);
lv_roller_set_options( ui_Day_heat_setpoint, "50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64\n65\n66\n67\n68\n69\n70\n71\n72\n73\n74", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Day_heat_setpoint, 100);
lv_obj_set_width( ui_Day_heat_setpoint, LV_SIZE_CONTENT);  /// 50
lv_obj_set_align( ui_Day_heat_setpoint, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Day_heat_setpoint, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Day_heat_setpoint, lv_color_hex(0xFF9040), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Day_heat_setpoint, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label14 = lv_label_create(ui_Day_heat_container);
lv_obj_set_width( ui_Label14, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label14, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label14, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label14,"");
lv_obj_set_style_text_color(ui_Label14, lv_color_hex(0xFF9040), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label14, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label14, &ui_font_MaterialSymbols24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Day_cool_container = lv_obj_create(ui_DAY);
lv_obj_remove_style_all(ui_Day_cool_container);
lv_obj_set_height( ui_Day_cool_container, 140);
lv_obj_set_width( ui_Day_cool_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Day_cool_container, LV_ALIGN_RIGHT_MID );
lv_obj_clear_flag( ui_Day_cool_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_blend_mode(ui_Day_cool_container, LV_BLEND_MODE_NORMAL, LV_PART_MAIN| LV_STATE_DISABLED);
lv_obj_set_style_opa(ui_Day_cool_container, 60, LV_PART_MAIN| LV_STATE_DISABLED);

ui_Label13 = lv_label_create(ui_Day_cool_container);
lv_obj_set_width( ui_Label13, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label13, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label13, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label13,"");
lv_obj_set_style_text_color(ui_Label13, lv_color_hex(0x4CAFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label13, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label13, &ui_font_MaterialSymbols24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Day_cool_setpoint = lv_roller_create(ui_Day_cool_container);
lv_roller_set_options( ui_Day_cool_setpoint, "50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64\n65\n66\n67\n68\n69\n70\n71\n72\n73\n74", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Day_cool_setpoint, 100);
lv_obj_set_width( ui_Day_cool_setpoint, LV_SIZE_CONTENT);  /// 50
lv_obj_set_align( ui_Day_cool_setpoint, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Day_cool_setpoint, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Day_cool_setpoint, lv_color_hex(0x4CAFFF), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Day_cool_setpoint, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Divider2 = lv_obj_create(ui_DAY);
lv_obj_set_width( ui_Divider2, 1);
lv_obj_set_height( ui_Divider2, 140);
lv_obj_set_x( ui_Divider2, 0 );
lv_obj_set_y( ui_Divider2, -3 );
lv_obj_set_align( ui_Divider2, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Divider2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_Divider2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Divider2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_Divider2, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_Divider2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_Divider2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_side(ui_Divider2, LV_BORDER_SIDE_LEFT, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Day_time_container = lv_obj_create(ui_DAY);
lv_obj_remove_style_all(ui_Day_time_container);
lv_obj_set_width( ui_Day_time_container, 130);
lv_obj_set_height( ui_Day_time_container, 140);
lv_obj_set_align( ui_Day_time_container, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Day_time_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Day_hrs_container = lv_obj_create(ui_Day_time_container);
lv_obj_remove_style_all(ui_Day_hrs_container);
lv_obj_set_height( ui_Day_hrs_container, 143);
lv_obj_set_width( ui_Day_hrs_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Day_hrs_container, LV_ALIGN_LEFT_MID );
lv_obj_clear_flag( ui_Day_hrs_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Day_hr = lv_roller_create(ui_Day_hrs_container);
lv_roller_set_options( ui_Day_hr, "12\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Day_hr, 100);
lv_obj_set_width( ui_Day_hr, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Day_hr, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Day_hr, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Day_hr, lv_color_hex(0x44474C), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Day_hr, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label16 = lv_label_create(ui_Day_time_container);
lv_obj_set_width( ui_Label16, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label16, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_Label16, 0 );
lv_obj_set_y( ui_Label16, -21 );
lv_obj_set_align( ui_Label16, LV_ALIGN_CENTER );
lv_label_set_text(ui_Label16,":");
lv_obj_set_style_text_font(ui_Label16, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Day_min_container = lv_obj_create(ui_Day_time_container);
lv_obj_remove_style_all(ui_Day_min_container);
lv_obj_set_height( ui_Day_min_container, 143);
lv_obj_set_width( ui_Day_min_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Day_min_container, LV_ALIGN_RIGHT_MID );
lv_obj_clear_flag( ui_Day_min_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Day_min = lv_roller_create(ui_Day_min_container);
lv_roller_set_options( ui_Day_min, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Day_min, 100);
lv_obj_set_width( ui_Day_min, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Day_min, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Day_min, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Day_min, lv_color_hex(0x44474C), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Day_min, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label17 = lv_label_create(ui_Day_time_container);
lv_obj_set_width( ui_Label17, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label17, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label17, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label17,"START (AM)");
lv_obj_set_style_text_color(ui_Label17, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label17, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label17, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_NIGHT = lv_tabview_add_tab(ui_TabView1, "NIGHT");
lv_obj_set_flex_flow(ui_NIGHT,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_NIGHT, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_obj_set_style_pad_left(ui_NIGHT, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_NIGHT, 10, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_NIGHT, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_NIGHT, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Night_heat_container = lv_obj_create(ui_NIGHT);
lv_obj_remove_style_all(ui_Night_heat_container);
lv_obj_set_height( ui_Night_heat_container, 140);
lv_obj_set_width( ui_Night_heat_container, LV_SIZE_CONTENT);  /// 2
lv_obj_set_align( ui_Night_heat_container, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Night_heat_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_blend_mode(ui_Night_heat_container, LV_BLEND_MODE_NORMAL, LV_PART_MAIN| LV_STATE_DISABLED);
lv_obj_set_style_opa(ui_Night_heat_container, 60, LV_PART_MAIN| LV_STATE_DISABLED);

ui_Night_heat_setpoint = lv_roller_create(ui_Night_heat_container);
lv_roller_set_options( ui_Night_heat_setpoint, "50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64\n65\n66\n67\n68\n69\n70\n71\n72\n73\n74", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Night_heat_setpoint, 100);
lv_obj_set_width( ui_Night_heat_setpoint, LV_SIZE_CONTENT);  /// 50
lv_obj_set_align( ui_Night_heat_setpoint, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Night_heat_setpoint, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Night_heat_setpoint, lv_color_hex(0xFF9040), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Night_heat_setpoint, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label15 = lv_label_create(ui_Night_heat_container);
lv_obj_set_width( ui_Label15, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label15, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label15, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label15,"");
lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0xFF9040), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label15, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label15, &ui_font_MaterialSymbols24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Night_cool_container = lv_obj_create(ui_NIGHT);
lv_obj_remove_style_all(ui_Night_cool_container);
lv_obj_set_height( ui_Night_cool_container, 140);
lv_obj_set_width( ui_Night_cool_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Night_cool_container, LV_ALIGN_RIGHT_MID );
lv_obj_clear_flag( ui_Night_cool_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_blend_mode(ui_Night_cool_container, LV_BLEND_MODE_NORMAL, LV_PART_MAIN| LV_STATE_DISABLED);
lv_obj_set_style_opa(ui_Night_cool_container, 60, LV_PART_MAIN| LV_STATE_DISABLED);

ui_Label18 = lv_label_create(ui_Night_cool_container);
lv_obj_set_width( ui_Label18, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label18, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label18, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label18,"");
lv_obj_set_style_text_color(ui_Label18, lv_color_hex(0x4CAFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label18, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label18, &ui_font_MaterialSymbols24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Night_cool_setpoint = lv_roller_create(ui_Night_cool_container);
lv_roller_set_options( ui_Night_cool_setpoint, "50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64\n65\n66\n67\n68\n69\n70\n71\n72\n73\n74", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Night_cool_setpoint, 100);
lv_obj_set_width( ui_Night_cool_setpoint, LV_SIZE_CONTENT);  /// 50
lv_obj_set_align( ui_Night_cool_setpoint, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Night_cool_setpoint, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Night_cool_setpoint, lv_color_hex(0x4CAFFF), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Night_cool_setpoint, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Divider3 = lv_obj_create(ui_NIGHT);
lv_obj_set_width( ui_Divider3, 1);
lv_obj_set_height( ui_Divider3, 140);
lv_obj_set_x( ui_Divider3, 0 );
lv_obj_set_y( ui_Divider3, -3 );
lv_obj_set_align( ui_Divider3, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Divider3, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_Divider3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Divider3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_Divider3, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_Divider3, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_Divider3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_side(ui_Divider3, LV_BORDER_SIDE_LEFT, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Night_time_container = lv_obj_create(ui_NIGHT);
lv_obj_remove_style_all(ui_Night_time_container);
lv_obj_set_width( ui_Night_time_container, 130);
lv_obj_set_height( ui_Night_time_container, 140);
lv_obj_set_align( ui_Night_time_container, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Night_time_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Night_hrs_container = lv_obj_create(ui_Night_time_container);
lv_obj_remove_style_all(ui_Night_hrs_container);
lv_obj_set_height( ui_Night_hrs_container, 143);
lv_obj_set_width( ui_Night_hrs_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Night_hrs_container, LV_ALIGN_LEFT_MID );
lv_obj_clear_flag( ui_Night_hrs_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Night_hr = lv_roller_create(ui_Night_hrs_container);
lv_roller_set_options( ui_Night_hr, "12\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Night_hr, 100);
lv_obj_set_width( ui_Night_hr, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Night_hr, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Night_hr, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Night_hr, lv_color_hex(0x44474C), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Night_hr, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label19 = lv_label_create(ui_Night_time_container);
lv_obj_set_width( ui_Label19, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label19, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_Label19, 0 );
lv_obj_set_y( ui_Label19, -21 );
lv_obj_set_align( ui_Label19, LV_ALIGN_CENTER );
lv_label_set_text(ui_Label19,":");
lv_obj_set_style_text_font(ui_Label19, &lv_font_montserrat_24, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Night_min_container = lv_obj_create(ui_Night_time_container);
lv_obj_remove_style_all(ui_Night_min_container);
lv_obj_set_height( ui_Night_min_container, 143);
lv_obj_set_width( ui_Night_min_container, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Night_min_container, LV_ALIGN_RIGHT_MID );
lv_obj_clear_flag( ui_Night_min_container, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_Night_min = lv_roller_create(ui_Night_min_container);
lv_roller_set_options( ui_Night_min, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL );
lv_obj_set_height( ui_Night_min, 100);
lv_obj_set_width( ui_Night_min, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_Night_min, LV_ALIGN_TOP_MID );
lv_obj_set_style_text_font(ui_Night_min, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_Night_min, lv_color_hex(0x44474C), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Night_min, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_Label20 = lv_label_create(ui_Night_time_container);
lv_obj_set_width( ui_Label20, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_Label20, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_Label20, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_Label20,"START (PM)");
lv_obj_set_style_text_color(ui_Label20, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_Label20, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_Label20, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_add_event_cb(ui_comp_get_child(ui_Schedule_header, UI_COMP_SETTING_HEADER_SETTING_BACK_BUTTON), ui_event_Schedule_header_Setting_back_button, LV_EVENT_ALL, NULL);
lv_obj_add_event_cb(ui_comp_get_child(ui_Schedule_header, UI_COMP_SETTING_HEADER_SETTING_ACCEPT_BUTTON), ui_event_Schedule_header_Setting_accept_button, LV_EVENT_ALL, NULL);
lv_obj_add_event_cb(ui_Schedule, ui_event_Schedule, LV_EVENT_ALL, NULL);

}