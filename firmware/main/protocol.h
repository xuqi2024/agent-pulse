// protocol.h — UART0 JSON line protocol between the host bridge and the
// firmware. Each message is a single JSON object terminated with '\n'.

#pragma once

#include "esp_err.h"

typedef enum {
    AP_PROTO_HELLO = 0,
    AP_PROTO_PING,
    AP_PROTO_STATE,
    AP_PROTO_TOOL_EVENT,
    AP_PROTO_CONFIG,
} ap_proto_type_t;

// Start the RX task on the configured UART.
esp_err_t ap_proto_init(void);

// Called from the render or button task to enqueue a TX message.
esp_err_t ap_proto_send_hello_ack(uint32_t fw_version);
esp_err_t ap_proto_send_state_ack(uint32_t seq);
esp_err_t ap_proto_send_pong(uint32_t seq);
esp_err_t ap_proto_send_btn(const char *name, uint32_t duration_ms);
esp_err_t ap_proto_send_error(const char *code, const char *detail);

// Apply a "config" message that came from the host.
void ap_proto_apply_config(int brightness, int theme, int rotation, int idle_anim, int fps_cap);
