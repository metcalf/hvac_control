#pragma once

#include "InputState.h"

#define ZONE_IO_TASK_STACK_SIZE 4096

void zone_io_init();

void zone_io_task(void *updateCb);

InputState zone_io_get_state();
