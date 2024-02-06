#pragma once

#include "ControllerDomain.h"

ControllerDomain::Config app_config_load();
void app_config_save(ControllerDomain::Config &config);
