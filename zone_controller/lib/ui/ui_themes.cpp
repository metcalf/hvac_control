// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.0
// LVGL version: 8.3.11
// Project name: zone_controller_ui

#include "ui.h"

const ui_theme_variable_t _ui_theme_color_border[1] = {0x909090};
const ui_theme_variable_t _ui_theme_alpha_border[1] = {255};

const ui_theme_variable_t _ui_theme_color_heat[1] = {0xFF9040};
const ui_theme_variable_t _ui_theme_alpha_heat[1] = {255};

const ui_theme_variable_t _ui_theme_color_cool[1] = {0x4CAFFF};
const ui_theme_variable_t _ui_theme_alpha_cool[1] = {255};

const ui_theme_variable_t _ui_theme_color_button_bg[1] = {0x44474C};
const ui_theme_variable_t _ui_theme_alpha_button_bg[1] = {255};
uint8_t ui_theme_idx = UI_THEME_DEFAULT;

void ui_theme_set(uint8_t theme_idx)
{
    ui_theme_idx = theme_idx;
    _ui_theme_set_variable_styles(UI_VARIABLE_STYLES_MODE_FOLLOW);
}
