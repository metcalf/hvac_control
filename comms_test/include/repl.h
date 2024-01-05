#pragma once

#include <esp_err.h>
#include <stdint.h>

typedef esp_err_t (*repl_set_speed_func)(uint8_t);

void repl_start(repl_set_speed_func set_speed);
