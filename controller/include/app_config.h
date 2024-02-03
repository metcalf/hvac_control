#pragma once

// TODO: Possible should move config definition elsewhere
#include "UIManager.h"

UIManager::Config app_config_load();
void app_config_save(UIManager::Config &config);
