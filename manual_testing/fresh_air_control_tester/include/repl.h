#pragma once

#include <esp_err.h>
#include <stdint.h>

typedef esp_err_t (*repl_set_speed_func)(uint8_t);
typedef esp_err_t (*repl_fetch_func)();

void repl_start(repl_set_speed_func set_speed, repl_fetch_func fetch);
