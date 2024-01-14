#pragma once

#include "InputState.h"

#define ZONE_IO_TASK_STACK_SIZE 2048

void zone_io_init();

void zone_io_task(void *);

void zone_io_log_state(InputState s);
bool zone_io_state_eq(InputState s1, InputState s2);
InputState zone_io_get_state();
