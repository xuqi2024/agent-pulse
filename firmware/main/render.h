// render.h — three screens (idle / processing / error), rendered from state.

#pragma once

#include "esp_err.h"
#include "lcd_panel.h"
#include "state.h"

// Lifecycle
esp_err_t ap_render_init(ap_lcd_t *lcd);
void ap_render_task(void *arg);   // pass NULL; calls vTaskDelete(NULL) on exit

// Force a full redraw on the next frame (used after a config change).
void ap_render_invalidate(void);

// Drawing primitives exposed for protocol/button use if needed.
void ap_draw_text(const ap_lcd_t *lcd, int x, int y, const char *s,
                  uint16_t color, uint8_t scale);
int  ap_text_width(const char *s, uint8_t scale);
int  ap_text_height(uint8_t scale);
void ap_draw_rect(const ap_lcd_t *lcd, int x, int y, int w, int h, uint16_t color);

// Common 16-bit RGB565 color constants
#define AP_RGB(r,g,b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) >> 3)))
#define AP_BLACK   0x0000
#define AP_WHITE   0xFFFF
#define AP_RED     AP_RGB(255, 80, 80)
#define AP_GREEN   AP_RGB(110, 227, 161)
#define AP_YELLOW  AP_RGB(255, 209, 102)
#define AP_BLUE    AP_RGB(80, 160, 255)
#define AP_PURPLE  AP_RGB(170, 130, 255)
#define AP_GRAY    AP_RGB(120, 130, 145)
#define AP_DARK    AP_RGB(14, 27, 44)
#define AP_DARK2   AP_RGB(27, 31, 42)
#define AP_DARK_ERR AP_RGB(42, 14, 18)
#define AP_CYAN    AP_RGB(80, 220, 220)
