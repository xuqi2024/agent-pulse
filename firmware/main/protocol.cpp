// protocol.cpp
// Minimal JSON-line parser tuned to our own protocol. The grammar is small
// enough that a 150-line state machine is more reliable (no mallocs, no
// library dependency) than pulling in cJSON.
//
// Recognized message shapes:
//   {"t":"hello", ...}
//   {"t":"ping","seq":N}
//   {"t":"state","status":"idle|processing|error|permission","tool":"...","message":"...","progress":N,"seq":N,"ts":N}
//   {"t":"tool_event","name":"...","phase":"start|end|error","target":"...","seq":N,"ts":N}
//   {"t":"config","brightness":N,"theme":"dark|light|auto","screen_rotation":N,"idle_animation":bool,"fps_cap":N}

#include "protocol.h"
#include "state.h"
#include "render.h"
#include "backlight.h"
#include "board_pins.h"
#include "log.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

// ---- line buffer ----------------------------------------------------------

#define LINE_MAX AP_MSG_MAX_LEN
static char s_line[LINE_MAX];
static int  s_line_len = 0;

// Forward decls
static void handle_line(char *line);
static void tx_string(const char *s);

// ---- helpers for our tiny parser -----------------------------------------

// Skip whitespace (in place of pointer advance). Returns new pointer.
static const char *skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

// Find next "key" : value pair. On success, returns pointer to char after the
// matched ':' and *out_key, *out_val point at the (untrimmed) key/value
// substrings. Returns NULL on end-of-object.
static const char *next_pair(const char *p, const char **out_key, int *klen,
                             const char **out_val, int *vlen)
{
    p = skip_ws(p);
    if (*p == '}' || *p == 0) return NULL;
    if (*p != '"') return NULL;
    p++;
    const char *kstart = p;
    while (*p && *p != '"') p++;
    if (*p != '"') return NULL;
    *out_key = kstart;
    *klen = (int)(p - kstart);
    p++;  // past closing quote
    p = skip_ws(p);
    if (*p != ':') return NULL;
    p++;
    p = skip_ws(p);
    const char *vstart = p;
    if (*p == '"') {
        vstart = ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        *out_val = vstart;
        *vlen = (int)(p - vstart);
        if (*p == '"') p++;
    } else {
        // number / bool / null — read until comma or '}'
        while (*p && *p != ',' && *p != '}') p++;
        *out_val = vstart;
        *vlen = (int)(p - vstart);
    }
    if (*p == ',') p++;
    return p;
}

static int streq(const char *s, int slen, const char *lit)
{
    int n = (int)strlen(lit);
    return slen == n && strncmp(s, lit, slen) == 0;
}

static int to_int(const char *s, int len)
{
    char buf[24]; int n = len < 23 ? len : 23;
    memcpy(buf, s, n); buf[n] = 0;
    return atoi(buf);
}

static void copy_str(char *dst, size_t dst_size, const char *src, int len)
{
    if (dst_size == 0) return;
    int n = (int)len < (int)dst_size - 1 ? (int)len : (int)dst_size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

// ---- message handlers -----------------------------------------------------

static void on_state(const char *v, int vlen)
{
    char buf[64]; copy_str(buf, sizeof(buf), v, vlen);
    if      (streq(buf, vlen, "idle"))       ap_state_set_status(AP_STATUS_IDLE);
    else if (streq(buf, vlen, "processing")) ap_state_set_status(AP_STATUS_PROCESSING);
    else if (streq(buf, vlen, "error"))      ap_state_set_status(AP_STATUS_ERROR);
    else if (streq(buf, vlen, "permission")) ap_state_set_status(AP_STATUS_ERROR);
}

static void on_config_apply(int brightness, int theme, int rotation,
                            int idle_anim, int fps_cap)
{
    if (brightness >= 0 && brightness <= 100) {
        ap_backlight_set((uint8_t)brightness);
    }
    (void)theme; (void)rotation; (void)idle_anim; (void)fps_cap;
    ap_render_invalidate();
}

// ---- parse top-level line -------------------------------------------------

static void handle_line(char *line)
{
    // Quick bail for obviously empty/invalid lines.
    const char *p = skip_ws(line);
    if (*p != '{') return;
    p++;
    p = skip_ws(p);
    if (*p != '"') return;
    p++;

    // First key: must be "t"
    const char *k; int kl;
    const char *v; int vl;
    p = next_pair(p, &k, &kl, &v, &vl);
    if (!p || !streq(k, kl, "t")) return;

    if (streq(v, vl, "hello")) {
        // payload: protocol_version, device_name, width, height
        ap_proto_send_hello_ack(0x00010000);  // 0.1.0
        ap_render_invalidate();
        return;
    }
    if (streq(v, vl, "ping")) {
        // pair: seq
        while ((p = next_pair(p, &k, &kl, &v, &vl))) {
            if (streq(k, kl, "seq")) {
                ap_proto_send_pong((uint32_t)to_int(v, vl));
            }
        }
        return;
    }
    if (streq(v, vl, "state")) {
        uint32_t seq = 0;
        while ((p = next_pair(p, &k, &kl, &v, &vl))) {
            if (streq(k, kl, "status"))       on_state(v, vl);
            else if (streq(k, kl, "tool"))     { char b[16]; copy_str(b, sizeof(b), v, vl); ap_state_set_tool(b); }
            else if (streq(k, kl, "message"))  { char b[60]; copy_str(b, sizeof(b), v, vl); ap_state_set_message(b); }
            else if (streq(k, kl, "progress")) ap_state_set_progress((uint8_t)to_int(v, vl));
            else if (streq(k, kl, "seq"))      seq = (uint32_t)to_int(v, vl);
        }
        ap_state_set_seq(seq);
        ap_proto_send_state_ack(seq);
        return;
    }
    if (streq(v, vl, "tool_event")) {
        // informational only — no state change required.
        return;
    }
    if (streq(v, vl, "config")) {
        int br = -1, th = -1, rot = -1, ia = -1, fps = -1;
        while ((p = next_pair(p, &k, &kl, &v, &vl))) {
            if (streq(k, kl, "brightness"))     br = to_int(v, vl);
            else if (streq(k, kl, "theme"))     th = to_int(v, vl);
            else if (streq(k, kl, "screen_rotation")) rot = to_int(v, vl);
            else if (streq(k, kl, "idle_animation")) ia = to_int(v, vl);
            else if (streq(k, kl, "fps_cap"))   fps = to_int(v, vl);
        }
        on_config_apply(br, th, rot, ia, fps);
        return;
    }
}

// ---- UART plumbing --------------------------------------------------------

static void on_uart_bytes(const uint8_t *buf, int n)
{
    for (int i = 0; i < n; i++) {
        char c = (char)buf[i];
        if (c == '\r') continue;
        if (c == '\n') {
            s_line[s_line_len] = 0;
            if (s_line_len > 0) handle_line(s_line);
            s_line_len = 0;
        } else {
            if (s_line_len < LINE_MAX - 1) s_line[s_line[s_line_len++] = c];
            else {
                // overflow: drop
                s_line_len = 0;
            }
        }
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    uint8_t *buf = (uint8_t *)malloc(AP_UART_RX_BUF);
    if (!buf) vTaskDelete(NULL);
    while (1) {
        int n = uart_read_bytes(BSP_PROTO_UART_NUM, buf, AP_UART_RX_BUF,
                                pdMS_TO_TICKS(50));
        if (n > 0) on_uart_bytes(buf, n);
    }
}

esp_err_t ap_proto_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BSP_PROTO_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(BSP_PROTO_UART_NUM,
                                        AP_UART_TX_BUF, AP_UART_RX_BUF,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BSP_PROTO_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BSP_PROTO_UART_NUM,
                                 BSP_PROTO_UART_TX, BSP_PROTO_UART_RX,
                                 -1, -1));

    xTaskCreate(rx_task, "ap_rx", 4096, NULL, 5, NULL);
    return ESP_OK;
}

// ---- TX -------------------------------------------------------------------

static void tx_string(const char *s)
{
    int n = (int)strlen(s);
    uart_write_bytes(BSP_PROTO_UART_NUM, s, n);
}

esp_err_t ap_proto_send_hello_ack(uint32_t fw_version)
{
    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"t\":\"hello_ack\",\"fw\":%u,\"w\":%d,\"h\":%d,\"caps\":[\"st7789\",\"bl_pwm\",\"boot_btn\"]}\n",
             (unsigned)fw_version, BSP_LCD_H_RES, BSP_LCD_V_RES);
    tx_string(buf);
    return ESP_OK;
}

esp_err_t ap_proto_send_state_ack(uint32_t seq)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"t\":\"state_ack\",\"seq\":%u,\"rendered\":true}\n", (unsigned)seq);
    tx_string(buf);
    return ESP_OK;
}

esp_err_t ap_proto_send_pong(uint32_t seq)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"t\":\"pong\",\"seq\":%u}\n", (unsigned)seq);
    tx_string(buf);
    return ESP_OK;
}

esp_err_t ap_proto_send_btn(const char *name, uint32_t duration_ms)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"t\":\"btn\",\"name\":\"%s\",\"duration_ms\":%u}\n",
             name ? name : "boot", (unsigned)duration_ms);
    tx_string(buf);
    return ESP_OK;
}

esp_err_t ap_proto_send_error(const char *code, const char *detail)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"t\":\"error\",\"code\":\"%s\",\"detail\":\"%s\"}\n",
             code ? code : "UNKNOWN", detail ? detail : "");
    tx_string(buf);
    return ESP_OK;
}

void ap_proto_apply_config(int brightness, int theme, int rotation, int idle_anim, int fps_cap)
{
    on_config_apply(brightness, theme, rotation, idle_anim, fps_cap);
}
