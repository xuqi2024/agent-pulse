// backlight.cpp

#include "backlight.h"
#include "board_pins.h"
#include "log.h"

#include "driver/ledc.h"

esp_err_t ap_backlight_init(void)
{
    const ledc_channel_config_t ch = {
        .gpio_num   = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BSP_BL_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = BSP_BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .flags      = { .output_invert = BSP_BL_LEDC_INVERT },
    };
    const ledc_timer_config_t tm = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BSP_BL_LEDC_RESOLUTION,
        .timer_num       = BSP_BL_LEDC_TIMER,
        .freq_hz         = BSP_BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t r;
    if ((r = ledc_timer_config(&tm)) != ESP_OK) return r;
    if ((r = ledc_channel_config(&ch)) != ESP_OK) return r;
    return ap_backlight_set(70);
}

esp_err_t ap_backlight_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    // 10-bit: 100% = 1023.
    uint32_t duty = (1023u * percent) / 100u;
    esp_err_t r;
    if ((r = ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_BL_LEDC_CHANNEL, duty)) != ESP_OK) return r;
    if ((r = ledc_update_duty(LEDC_LOW_SPEED_MODE, BSP_BL_LEDC_CHANNEL)) != ESP_OK) return r;
    return ESP_OK;
}
