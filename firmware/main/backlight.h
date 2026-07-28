// backlight.h — LEDC PWM dimming of the LCD backlight.
// Wiki ref: section 9.3 — LEDC channel 0, timer 1, 5 kHz, 10-bit, inverted.

#pragma once

#include "esp_err.h"

esp_err_t ap_backlight_init(void);

// Set brightness 0..100 (clamped). 0 turns the backlight off; 100 = full on.
esp_err_t ap_backlight_set(uint8_t percent);
