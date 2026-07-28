// log.h — silent-by-default logging
// We silence ESP_LOG to keep the UART0 protocol stream clean. If a developer
// wants to see the boot log, attach a USB-Serial/JTAG probe (different port
// on the SZPI-ESP32S3) or temporarily re-enable CONFIG_LOG_DEFAULT_LEVEL_INFO.

#pragma once

#include "esp_log.h"

#ifdef AP_QUIET
  #define AP_LOGI(tag, fmt, ...)  do {} while (0)
  #define AP_LOGW(tag, fmt, ...)  do {} while (0)
  #define AP_LOGE(tag, fmt, ...)  do { ets_printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
  #define AP_LOGD(tag, fmt, ...)  do {} while (0)
#else
  #define AP_LOGI(tag, fmt, ...)  ESP_LOGI(tag, fmt, ##__VA_ARGS__)
  #define AP_LOGW(tag, fmt, ...)  ESP_LOGW(tag, fmt, ##__VA_ARGS__)
  #define AP_LOGE(tag, fmt, ...)  ESP_LOGE(tag, fmt, ##__VA_ARGS__)
  #define AP_LOGD(tag, fmt, ...)  ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#endif

#define AP_TAG "agent-pulse"
