// buttons.cpp

#include "buttons.h"
#include "protocol.h"
#include "state.h"
#include "render.h"
#include "board_pins.h"
#include "log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static inline bool read_btn(void)
{
    int v = gpio_get_level(BSP_BOOT_BUTTON);
    return BSP_BOOT_BUTTON_ACTIVE_LOW ? (v == 0) : (v != 0);
}

esp_err_t ap_buttons_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BSP_BOOT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = BSP_BOOT_BUTTON_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = BSP_BOOT_BUTTON_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

void ap_buttons_task(void *arg)
{
    (void)arg;
    int64_t pressed_since = 0;
    bool was_pressed = false;
    bool long_reported = false;

    while (1) {
        bool pressed = read_btn();
        int64_t now = esp_timer_get_time() / 1000;

        if (pressed && !was_pressed) {
            pressed_since = now;
            long_reported = false;
        }
        if (pressed && !long_reported && (now - pressed_since) > 1000) {
            // long press: clear state and force hello
            ap_state_t empty = {};
            empty.status = AP_STATUS_IDLE;
            empty.progress = 255;
            ap_state_set(&empty);
            ap_proto_send_btn("boot_long", (uint32_t)(now - pressed_since));
            ap_proto_send_hello_ack(0x00010000);
            ap_render_invalidate();
            long_reported = true;
        }
        if (!pressed && was_pressed) {
            // released
            if (!long_reported && (now - pressed_since) > 30) {
                ap_proto_send_btn("boot", (uint32_t)(now - pressed_since));
            }
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
