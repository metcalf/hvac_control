SET(SOURCES components/ui_comp_setting_header.c
    components/ui_comp.c
    screens/ui_Boot.c
    screens/ui_Home.c
    screens/ui_Fan_speed_override.c
    screens/ui_Thermostat.c
    screens/ui_CO2.c
    screens/ui_Schedule.c
    screens/ui_Settings.c
    screens/ui_Equipment.c
    screens/ui_Temp_limits.c
    screens/ui_Temp_offsets.c
    screens/ui_Wifi.c
    screens/ui_Restart.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.cpp
    fonts/ui_font_MaterialSymbols18.c
    fonts/ui_font_MaterialSymbols24.c)

add_library(ui ${SOURCES})
