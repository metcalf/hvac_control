#pragma once

#include <stdint.h>
#include <time.h>

#include "esp_err.h"

esp_err_t rtc_rx8111_init_client(bool *hasTime);
esp_err_t rtc_rx8111_get_time(struct tm *dt);
esp_err_t rtc_rx8111_set_time(struct tm *dt);
