SET(SOURCES screens/ui_Screen1.c
    ui_theme_manager.cpp
    ui_themes.cpp
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.cpp
    fonts/ui_font_MaterialSymbols18.c
    fonts/ui_font_MaterialSymbols24.c
    fonts/ui_font_MaterialSymbols36.c)

add_library(ui ${SOURCES})
