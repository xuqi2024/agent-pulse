// lcd_panel.h — thin wrapper around the esp_lcd ST7789 driver, configured
// per the wiki's section 9.3 init sequence for the 立创·实战派 ESP32-S3.

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
} ap_lcd_t;

// Initialize SPI bus, attach ST7789, and apply the orientation tweaks that
// the wiki section 9.3 calls out (color invert, swap_xy, mirror).
// Backlight GPIO is configured separately by ap_backlight_init().
esp_err_t ap_lcd_init(ap_lcd_t *out);

// Convenience: draw a solid rectangle of width x height pixels in `color`
// (RGB565), at (x0, y0). Uses an internal PSRAM row buffer.
esp_err_t ap_lcd_fill(ap_lcd_t *lcd, int x0, int y0, int w, int h, uint16_t color);

// Draw a raw RGB565 buffer of (w*h) pixels at (x0, y0). Caller owns the
// buffer and must keep it alive until this returns.
esp_err_t ap_lcd_blit(ap_lcd_t *lcd, int x0, int y0, int w, int h, const uint16_t *pixels);

// Dimensions of the panel (320 x 240 in our config).
int ap_lcd_width(void);
int ap_lcd_height(void);
