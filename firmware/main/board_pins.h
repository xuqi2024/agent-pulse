// board_pins.h
// Pin definitions for 立创·实战派 ESP32-S3 (SZPI-ESP32S3)
// Reference: https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html
//            https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lcd-display.html
//
// IMPORTANT: I2C pins for the on-board IO expander are not in the wiki
// introduction. The defaults below are best-guess. If `ap_ioexp_init()`
// fails, run `i2cdetect` from a separate sketch, or edit these two macros.

#pragma once

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_lcd_panel_vendor.h"

// ---- ST7789 LCD (2.0" IPS, 320x240) ---------------------------------------
// Wiki section 9.3: SPI3_HOST, MOSI=GPIO40, CLK=GPIO41, DC=GPIO39, CS=via
// I/O expander (treated as NC by the SPI driver), RST=NC, BL=GPIO42 (PWM).
#define BSP_LCD_SPI_NUM            (SPI3_HOST)
#define BSP_LCD_SPI_MOSI           (GPIO_NUM_40)
#define BSP_LCD_SPI_CLK            (GPIO_NUM_41)
#define BSP_LCD_DC                 (GPIO_NUM_39)
#define BSP_LCD_SPI_CS             (GPIO_NUM_NC)   // driven by I/O expander
#define BSP_LCD_RST                (GPIO_NUM_NC)   // tied to chip reset on board
#define BSP_LCD_BACKLIGHT          (GPIO_NUM_42)

#define BSP_LCD_H_RES              (320)
#define BSP_LCD_V_RES              (240)
#define BSP_LCD_PIXEL_CLOCK_HZ     (80 * 1000 * 1000)
#define BSP_LCD_BITS_PER_PIXEL     (16)
#define BSP_LCD_CMD_BITS           (8)
#define BSP_LCD_PARAM_BITS         (8)

// Required by the wiki's init sequence for this exact panel
#define BSP_LCD_INVERT_COLOR       (true)
#define BSP_LCD_SWAP_XY            (true)
#define BSP_LCD_MIRROR_X           (true)
#define BSP_LCD_MIRROR_Y           (false)

// ---- UART for bridge protocol (CH340K path) -------------------------------
// CH340K on the board is wired to ESP32-S3 UART0 (GPIO43=TX, GPIO44=RX).
// We disable the ROM console so this whole UART is free for our JSON protocol.
#define BSP_PROTO_UART_NUM         (UART_NUM_0)
#define BSP_PROTO_UART_TX          (GPIO_NUM_43)
#define BSP_PROTO_UART_RX          (GPIO_NUM_44)
#define BSP_PROTO_UART_BAUD        (115200)

// ---- I/O expander (drives LCD_CS, PA_EN, DVP_PWDN) --------------------------
// Wiki says it is an 8-bit I2C expander but does not name the chip.
// Common addresses for PCF8574 family: 0x20..0x27. AW9523 is 0x58.
// Edit if your board variant uses different pins.
#define BSP_IOEXP_I2C_NUM          (I2C_NUM_0)
#define BSP_IOEXP_I2C_SDA          (GPIO_NUM_1)    // <-- verify on your board
#define BSP_IOEXP_I2C_SCL          (GPIO_NUM_2)    // <-- verify on your board
#define BSP_IOEXP_I2C_FREQ_HZ      (100 * 1000)
#define BSP_IOEXP_PROBE_ADDRS      { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x70, 0x58 }
#define BSP_IOEXP_LCD_CS_BIT       (0)             // bit 0 of the expander

// ---- Boot button -----------------------------------------------------------
// On SZPI-ESP32S3, the user button is wired to GPIO0 (BOOT button) and is
// usable as a normal GPIO once the auto-download circuit releases it.
#define BSP_BOOT_BUTTON            (GPIO_NUM_0)
#define BSP_BOOT_BUTTON_ACTIVE_LOW (true)

// ---- Backlight PWM ---------------------------------------------------------
// Wiki section 9.3: LEDC channel 0, timer 1, 5 kHz, 10-bit, output inverted.
#define BSP_BL_LEDC_TIMER          (LEDC_TIMER_1)
#define BSP_BL_LEDC_CHANNEL        (LEDC_CHANNEL_0)
#define BSP_BL_LEDC_FREQ_HZ        (5000)
#define BSP_BL_LEDC_RESOLUTION     (LEDC_TIMER_10_BIT)
#define BSP_BL_LEDC_INVERT         (1)

// ---- Misc ------------------------------------------------------------------
#define AP_FPS_CAP                 (20)            // render frame cap
#define AP_WATCHDOG_TIMEOUT_MS     (5000)          // no-host-msg threshold
#define AP_UART_RX_BUF             (1024)
#define AP_UART_TX_BUF             (1024)
#define AP_MSG_MAX_LEN             (1024)          // max line length for protocol
