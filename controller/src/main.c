#include "i2c_manager.h"
#include "lvgl_helpers.h"

#include "controller_main.h"
#include "init_display.h"

// lvgl_esp32_drivers causes compilation problems when included from C++ so I use this
// little C shim to initialize lvgl before starting the app
void app_main() {
    lvgl_i2c_locking(i2c_manager_locking());
    init_display();

    controller_main();
}
