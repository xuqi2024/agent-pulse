// render.cpp
// Three screens + breathing dot spinner. Frame budget at 20 fps is 50 ms.

#include "render.h"
#include "font_5x7.h"
#include "log.h"
#include "board_pins.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

static ap_lcd_t s_lcd;
static volatile bool s_invalidate = true;
static volatile ap_status_t s_last_drawn = AP_STATUS_BOOT;
static int64_t s_last_draw_us = 0;
static int64_t s_anim_start_us = 0;

// --- helpers ---------------------------------------------------------------

static inline int s_min(int a, int b) { return a < b ? a : b; }
static inline int s_max(int a, int b) { return a > b ? a : b; }

void ap_draw_rect(const ap_lcd_t *lcd, int x, int y, int w, int h, uint16_t color)
{
    ap_lcd_fill((ap_lcd_t *)lcd, x, y, w, h, color);
}

void ap_draw_text(const ap_lcd_t *lcd, int x, int y, const char *s,
                  uint16_t color, uint8_t scale)
{
    if (!s) return;
    int cw = AP_FONT_W, ch = AP_FONT_H;
    int dx = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < AP_FONT_FIRST || c > AP_FONT_LAST) { s++; dx += (cw + 1) * scale; continue; }
        const uint8_t *glyph = ap_font_5x7[c - AP_FONT_FIRST];
        for (int col = 0; col < cw; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < ch; row++) {
                if (bits & (1u << row)) {
                    if (scale == 1) {
                        uint16_t px[1] = { color };
                        ap_lcd_blit((ap_lcd_t *)lcd, x + dx + col, y + row, 1, 1, px);
                    } else {
                        ap_lcd_fill((ap_lcd_t *)lcd,
                                    x + dx + col * scale, y + row * scale,
                                    scale, scale, color);
                    }
                }
            }
        }
        s++;
        dx += (cw + 1) * scale;
    }
}

int ap_text_width(const char *s, uint8_t scale)
{
    if (!s) return 0;
    int n = 0;
    while (*s++) n++;
    return n * (AP_FONT_W + 1) * scale - scale;  // last char has no trailing space
}

int ap_text_height(uint8_t scale) { return AP_FONT_H * scale; }

void ap_render_invalidate(void) { s_invalidate = true; }

// --- top-bar / status helpers ---------------------------------------------

static void draw_topbar(uint16_t bg, uint16_t fg, const char *title)
{
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, 22, bg);
    ap_draw_text(&s_lcd, 6, 8, title, fg, 1);
}

static void draw_centered(int y, const char *s, uint16_t color, uint8_t scale)
{
    int w = ap_text_width(s, scale);
    int x = (BSP_LCD_H_RES - w) / 2;
    ap_draw_text(&s_lcd, x, y, s, color, scale);
}

// --- three screens ---------------------------------------------------------

static void render_idle(int64_t now_us)
{
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, AP_DARK);
    draw_topbar(AP_DARK, AP_GREEN, "*  agent-pulse  *");

    // breathing dot in upper area
    float t = (float)(now_us - s_anim_start_us) / 1e6f;
    float phase = (sinf(t * 2.0f) + 1.0f) * 0.5f;  // 0..1
    int dot_r = 6 + (int)(phase * 4.0f);
    int cx = BSP_LCD_H_RES / 2;
    int cy = 80;
    ap_draw_rect(&s_lcd, cx - dot_r, cy - dot_r, dot_r * 2, dot_r * 2, AP_GREEN);

    draw_centered(120, "OK",       AP_GREEN,  3);
    draw_centered(150, "standby",  AP_GRAY,   2);

    // bottom hint
    char hint[48];
    snprintf(hint, sizeof(hint), "waiting for agent ...");
    draw_centered(210, hint, AP_GRAY, 1);
}

static void render_processing(int64_t now_us, const ap_state_t *st)
{
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, AP_DARK2);
    draw_topbar(AP_DARK2, AP_YELLOW, ">>>  AGENT RUNNING");

    // spinner at top-right of body
    float t = (float)(now_us - s_anim_start_us) / 1e6f;
    int ang = ((int)(t * 240.0f)) % 360;
    int sx = BSP_LCD_H_RES - 24, sy = 40;
    for (int i = 0; i < 8; i++) {
        int a = (ang + i * 45) % 360;
        float rad = (float)a * 3.14159f / 180.0f;
        int dx = (int)(cosf(rad) * 8.0f);
        int dy = (int)(sinf(rad) * 8.0f);
        uint16_t c = AP_YELLOW;
        if (i > 0) c = AP_GRAY;
        ap_draw_rect(&s_lcd, sx + dx - 1, sy + dy - 1, 3, 3, c);
    }

    // tool name (large)
    char line[40];
    snprintf(line, sizeof(line), "tool:  %s", st->tool[0] ? st->tool : "agent");
    ap_draw_text(&s_lcd, 8, 40, line, AP_YELLOW, 2);

    // message (scroll if too long)
    int msg_y = 80;
    int max_w = BSP_LCD_H_RES - 16;
    char msg[sizeof(st->message) + 4];
    strncpy(msg, st->message, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = 0;
    int msgw = ap_text_width(msg, 1);
    int offset = 0;
    if (msgw > max_w) {
        // scroll: shift left by an integer that advances over time
        int total = msgw - max_w + 16;
        int period_ms = 6000;
        int phase = (int)((now_us - s_anim_start_us) / 1000) % period_ms;
        if (phase < (period_ms * 2 / 3)) {
            offset = 0;
        } else {
            int scroll_ms = period_ms / 3;
            int t_ms = phase - (period_ms * 2 / 3);
            offset = (t_ms * total) / scroll_ms;
            if (offset > total) offset = total;
        }
        // truncate
        char buf[80];
        snprintf(buf, sizeof(buf), "%s   ", msg);
        strncpy(msg, buf, sizeof(msg) - 1);
        msg[sizeof(msg) - 1] = 0;
    }
    ap_draw_text(&s_lcd, 8 - offset, msg_y, msg, AP_WHITE, 1);

    // progress bar
    if (st->progress != 255) {
        int bar_x = 8, bar_y = 200, bar_w = BSP_LCD_H_RES - 16, bar_h = 14;
        ap_draw_rect(&s_lcd, bar_x, bar_y, bar_w, bar_h, AP_DARK);
        int fill = (bar_w * st->progress) / 100;
        if (fill > 0) ap_draw_rect(&s_lcd, bar_x, bar_y, fill, bar_h, AP_YELLOW);
        // percent text
        char pct[8]; snprintf(pct, sizeof(pct), "%d%%", st->progress);
        ap_draw_text(&s_lcd, bar_x + bar_w - ap_text_width(pct, 1) - 4,
                     bar_y - 10, pct, AP_YELLOW, 1);
    }
}

static void render_error(int64_t now_us, const ap_state_t *st)
{
    (void)now_us;
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, AP_DARK_ERR);
    draw_topbar(AP_DARK_ERR, AP_RED, "!!!  AGENT BLOCKED");

    draw_centered(50, "PERMISSION", AP_RED, 3);
    draw_centered(90, "REQUIRED",   AP_RED, 3);

    // message (truncate to one line)
    char msg[40]; strncpy(msg, st->message, sizeof(msg) - 1); msg[sizeof(msg) - 1] = 0;
    draw_centered(150, msg, AP_WHITE, 1);

    ap_draw_text(&s_lcd, 30, 210, "[press BOOT to clear]", AP_GRAY, 1);
}

static void render_no_link(int64_t now_us)
{
    (void)now_us;
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, AP_DARK);
    draw_topbar(AP_DARK, AP_RED, "!!  NO LINK");

    draw_centered(80,  "CONNECTION", AP_RED, 3);
    draw_centered(120, "LOST",       AP_RED, 3);
    draw_centered(180, "check USB cable", AP_GRAY, 1);
    ap_draw_text(&s_lcd, 30, 215, "[press BOOT to retry]", AP_GRAY, 1);
}

static void render_boot(int64_t now_us)
{
    (void)now_us;
    ap_draw_rect(&s_lcd, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, AP_DARK);
    draw_topbar(AP_DARK, AP_CYAN, "*  agent-pulse  *");
    draw_centered(120, "booting ...", AP_CYAN, 2);
}

// --- public ----------------------------------------------------------------

esp_err_t ap_render_init(ap_lcd_t *lcd)
{
    if (!lcd) return ESP_ERR_INVALID_ARG;
    s_lcd = *lcd;
    s_anim_start_us = esp_timer_get_time();
    s_invalidate = true;
    s_last_drawn = AP_STATUS_BOOT;
    return ESP_OK;
}

void ap_render_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);

    // initial draw
    render_boot(esp_timer_get_time());
    s_last_drawn = AP_STATUS_BOOT;

    const int period_ms = 1000 / AP_FPS_CAP;
    int64_t last_frame_us = esp_timer_get_time();

    while (1) {
        int64_t now = esp_timer_get_time();
        ap_state_t st;
        ap_state_get(&st);

        // Watchdog: if we have not heard from host recently, override status.
        if (st.status != AP_STATUS_BOOT && ap_state_stale_ms(AP_WATCHDOG_TIMEOUT_MS)) {
            st.status = AP_STATUS_NO_CONNECTION;
        }

        bool force = s_invalidate || (st.status != s_last_drawn);
        bool animating = (st.status == AP_STATUS_IDLE || st.status == AP_STATUS_PROCESSING);

        if (force || animating) {
            switch (st.status) {
                case AP_STATUS_BOOT:          render_boot(now);           break;
                case AP_STATUS_IDLE:          render_idle(now);           break;
                case AP_STATUS_PROCESSING:    render_processing(now, &st);break;
                case AP_STATUS_ERROR:         render_error(now, &st);     break;
                case AP_STATUS_NO_CONNECTION: render_no_link(now);        break;
            }
            s_invalidate = false;
            s_last_drawn = st.status;
            last_frame_us = now;
        }
        // (s_last_draw_us is currently unused outside the static path; keep for future)
        s_last_draw_us = now;
        esp_task_wdt_reset();

        // Sleep to next frame slot
        int64_t elapsed_ms = (now - last_frame_us) / 1000;
        int sleep_ms = (int)period_ms - (int)elapsed_ms;
        if (sleep_ms < 0) sleep_ms = 0;
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}
