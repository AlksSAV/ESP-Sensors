/*
 * Display Driver for Waveshare ESP32-C6-LCD-1.47
 * ST7789 LCD controller via SPI
 * 172x320 resolution
 */

#pragma once

#include <driver/spi_master.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <lvgl.h>
#include <esp_err.h>

#define LCD_HOST_SPI SPI2_HOST
#define LCD_PIN_MOSI 6
#define LCD_PIN_SCLK 7
#define LCD_PIN_CS 14
#define LCD_PIN_DC 15
#define LCD_PIN_RST 21
#define LCD_PIN_BL 22

#define LCD_BACKLIGHT_BRIGHTNESS 128  /* 50% brightness (0-255) */
#define LCD_WIDTH 172
#define LCD_HEIGHT 320

/* ST7789 имеет GRAM 240x320, а видимая область 172x320 -> смещение по X */
#define LCD_OFFSET_X 34
#define LCD_OFFSET_Y 0

esp_err_t display_init(void);
esp_err_t display_backlight_set(uint8_t brightness);
void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
