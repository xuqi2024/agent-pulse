// buttons.h — handle the BOOT button (single click = report; long press =
// reset state and force a hello).

#pragma once

#include "esp_err.h"

esp_err_t ap_buttons_init(void);
void ap_buttons_task(void *arg);
