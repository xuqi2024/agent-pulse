// main.cpp — agent-pulse firmware entry point.
//
// Initialization order (matches the wiki's "液晶显示" chapter):
//   1. I/O expander (pulls LCD_CS low via the on-board I2C expander)
//   2. ST7789 panel via SPI3
//   3. LEDC backlight
//   4. UART0 protocol (CH340K path on the board)
//   5. Boot button + global state
//   6. FreeRTOS tasks: rx / render / buttons
//
// Console output is intentionally suppressed: see sdkconfig.defaults. To
// diagnose the boot, temporarily flip CONFIG_LOG_DEFAULT_LEVEL_INFO=y and
// re-flash; the log goes to the same UART and is interleaved with our
// protocol — only do this when no host bridge is running.

#include "io_expander.h"
#include "lcd_panel.h"
#include "backlight.h"
#include "state.h"
#include "render.h"
#include "protocol.h"
#include "buttons.h"
#include "log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"

extern "C" void app_main(void)
{
    AP_LOGI(AP_TAG, "agent-pulse booting");

    // NVS isn't used yet but tinyusb-stack may need it on some variants.
    nvs_flash_init();

    ap_state_init();

    // 1) I/O expander (LCD_CS line)
    ap_ioexp_info_t ioexp;
    esp_err_t r = ap_ioexp_init(&ioexp);
    if (r != ESP_OK) {
        AP_LOGE(AP_TAG, "I/O expander not found — LCD may be unresponsive. "
                        "Run ap_ioexp_scan_and_print() to find addresses.");
    }

    // 2) LCD panel
    ap_lcd_t lcd;
    r = ap_lcd_init(&lcd);
    if (r != ESP_OK) {
        AP_LOGE(AP_TAG, "lcd init failed: %s", esp_err_to_name(r));
        // Continue — UART path is still useful for diagnostics.
    }

    // 3) Backlight
    ap_backlight_init();

    // 4) Protocol
    ap_proto_init();
    ap_proto_send_hello_ack(0x00010000);

    // 5) Buttons
    ap_buttons_init();

    // 6) Render
    ap_render_init(&lcd);
    xTaskCreate(ap_render_task, "ap_render", 8192, NULL, 1, NULL);
    xTaskCreate(ap_buttons_task, "ap_btn",    2048, NULL, 1, NULL);

    AP_LOGI(AP_TAG, "agent-pulse ready");
}
