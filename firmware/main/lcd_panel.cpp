// lcd_panel.cpp
// Wiki ref: https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lcd-display.html

#include "lcd_panel.h"
#include "board_pins.h"
#include "log.h"

#include "driver/spi_common.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_heap_caps.h"
#include <string.h>

int ap_lcd_width(void)  { return BSP_LCD_H_RES; }
int ap_lcd_height(void) { return BSP_LCD_V_RES; }

esp_err_t ap_lcd_init(ap_lcd_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    // 1) SPI bus
    spi_bus_config_t buscfg = {
        .sclk_io_num     = BSP_LCD_SPI_CLK,
        .mosi_io_num     = BSP_LCD_SPI_MOSI,
        .miso_io_num     = GPIO_NUM_NC,
        .quadwp_io_num   = GPIO_NUM_NC,
        .quadhd_io_num   = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        AP_LOGE("lcd", "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2) Panel IO
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = BSP_LCD_DC,
        .cs_gpio_num       = BSP_LCD_SPI_CS,   // NC; CS held low by I/O expander
        .pclk_hz           = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = BSP_LCD_CMD_BITS,
        .lcd_param_bits    = BSP_LCD_PARAM_BITS,
        .spi_mode          = 2,                // ST7789 mode 2 on this board
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &out->io);
    if (ret != ESP_OK) {
        AP_LOGE("lcd", "new_panel_io_spi: %s", esp_err_to_name(ret));
        spi_bus_free(BSP_LCD_SPI_NUM);
        return ret;
    }

    // 3) ST7789 driver
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,            // NC on this board
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ret = esp_lcd_new_panel_st7789(out->io, &panel_config, &out->panel);
    if (ret != ESP_OK) {
        AP_LOGE("lcd", "new_panel_st7789: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(out->io);
        spi_bus_free(BSP_LCD_SPI_NUM);
        return ret;
    }

    // 4) Reset + init + wiki-mandated orientation
    ESP_ERROR_CHECK(esp_lcd_panel_reset(out->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(out->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(out->panel, BSP_LCD_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(out->panel, BSP_LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(out->panel, BSP_LCD_MIRROR_X, BSP_LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(out->panel, true));

    AP_LOGI("lcd", "ST7789 320x240 ready (SPI3, mode 2, swap+mirror, invert)");
    return ESP_OK;
}

esp_err_t ap_lcd_fill(ap_lcd_t *lcd, int x0, int y0, int w, int h, uint16_t color)
{
    if (!lcd || !lcd->panel || w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;
    int W = ap_lcd_width();
    if (x0 < 0) { w += x0; x0 = 0; }
    if (y0 < 0) { h += y0; y0 = 0; }
    if (x0 + w > W) w = W - x0;
    if (y0 + h > ap_lcd_height()) h = ap_lcd_height() - y0;
    if (w <= 0 || h <= 0) return ESP_OK;

    uint16_t *row = (uint16_t *)heap_caps_malloc(w * sizeof(uint16_t),
                                                 MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (!row) return ESP_ERR_NO_MEM;
    for (int i = 0; i < w; i++) row[i] = color;
    for (int y = 0; y < h; y++) {
        esp_err_t r = esp_lcd_panel_draw_bitmap(lcd->panel, x0, y0 + y, x0 + w, y0 + y + 1, row);
        if (r != ESP_OK) { free(row); return r; }
    }
    free(row);
    return ESP_OK;
}

esp_err_t ap_lcd_blit(ap_lcd_t *lcd, int x0, int y0, int w, int h, const uint16_t *pixels)
{
    if (!lcd || !lcd->panel || w <= 0 || h <= 0 || !pixels) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(lcd->panel, x0, y0, x0 + w, y0 + h, pixels);
}
