#pragma once

#include "InputState.h"
#include "esp_err.h"

void zone_io_init();

void zone_io_task(void *);

InputState zone_io_get_state();
