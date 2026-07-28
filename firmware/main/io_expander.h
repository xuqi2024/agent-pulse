// io_expander.h — drives the on-board I2C 8-bit I/O expander that controls
// LCD_CS (active low), PA_EN (audio amp), and DVP_PWDN (camera).
//
// The board uses an 8-bit I2C expander whose model is not specified in the
// wiki. We probe the common PCF8574 / PCF8575 / AW9523 addresses at boot.

#pragma once

#include "esp_err.h"

typedef struct {
    uint8_t i2c_addr;        // 7-bit address
    uint8_t lcd_cs_bit;      // bit index that drives LCD_CS
    bool    active_low;      // LCD chip-select is active-low on the board
} ap_ioexp_info_t;

// Initialize I2C bus, probe for an I/O expander, and pull LCD_CS low so the
// ST7789 panel is selected. On success returns ESP_OK and fills `info`.
// On failure (no expander found at the candidate addresses) returns
// ESP_ERR_NOT_FOUND and the LCD will not respond — caller should retry or
// fall back to a direct GPIO.
esp_err_t ap_ioexp_init(ap_ioexp_info_t *info);

// Optional: print the I2C scan results to the console (used by ap_doctor).
void ap_ioexp_scan_and_print(void);
