// io_expander.cpp

#include "io_expander.h"
#include "board_pins.h"
#include "log.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include <string.h>

static const uint8_t kProbeAddrs[] = BSP_IOEXP_PROBE_ADDRS;
static const int kProbeCount = sizeof(kProbeAddrs) / sizeof(kProbeAddrs[0]);

static bool probe_addr(i2c_port_t num, uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true /*ACK*/);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(num, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

static esp_err_t write_byte(i2c_port_t num, uint8_t addr, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(num, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void ap_ioexp_scan_and_print(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_IOEXP_I2C_SDA,
        .scl_io_num = BSP_IOEXP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed_hz = BSP_IOEXP_I2C_FREQ_HZ },
    };
    ESP_ERROR_CHECK(i2c_param_config(BSP_IOEXP_I2C_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(BSP_IOEXP_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0));

    AP_LOGI("ioexp", "I2C scan on SDA=GPIO%d SCL=GPIO%d:", BSP_IOEXP_I2C_SDA, BSP_IOEXP_I2C_SCL);
    int found = 0;
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (probe_addr(BSP_IOEXP_I2C_NUM, addr)) {
            AP_LOGI("ioexp", "  found device at 0x%02x", addr);
            found++;
        }
    }
    if (!found) AP_LOGW("ioexp", "  no I2C devices responded. Check pin map.");
    i2c_driver_delete(BSP_IOEXP_I2C_NUM);
}

esp_err_t ap_ioexp_init(ap_ioexp_info_t *info)
{
    if (!info) return ESP_ERR_INVALID_ARG;
    memset(info, 0, sizeof(*info));

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_IOEXP_I2C_SDA,
        .scl_io_num = BSP_IOEXP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed_hz = BSP_IOEXP_I2C_FREQ_HZ },
    };
    ESP_ERROR_CHECK(i2c_param_config(BSP_IOEXP_I2C_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(BSP_IOEXP_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0));

    for (int i = 0; i < kProbeCount; i++) {
        uint8_t addr = kProbeAddrs[i];
        if (probe_addr(BSP_IOEXP_I2C_NUM, addr)) {
            info->i2c_addr    = addr;
            info->lcd_cs_bit  = BSP_IOEXP_LCD_CS_BIT;
            info->active_low  = true;

            // Default output: all bits high (most boards idle this way).
            // Then pull LCD_CS bit low to select the ST7789.
            uint8_t val = 0xFF;
            if (info->active_low) val &= ~(1u << info->lcd_cs_bit);
            esp_err_t wr = write_byte(BSP_IOEXP_I2C_NUM, addr, val);
            if (wr != ESP_OK) {
                AP_LOGW("ioexp", "addr 0x%02x probed but write failed: %s", addr, esp_err_to_name(wr));
                continue;
            }
            AP_LOGI("ioexp", "I/O expander at 0x%02x, LCD_CS bit=%d (active-low)",
                     addr, info->lcd_cs_bit);
            return ESP_OK;
        }
    }
    AP_LOGE("ioexp", "no I/O expander found in candidate addresses");
    return ESP_ERR_NOT_FOUND;
}
