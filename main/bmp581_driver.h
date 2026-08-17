/*
 * BMP581 Pressure and Temperature Sensor Driver
 * SPI interface
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

typedef struct {
    float pressure;     // hPa (hectopascals)
    float temperature;  // Celsius
} bmp581_data_t;

esp_err_t bmp581_init(int cs_pin);
esp_err_t bmp581_read(bmp581_data_t *data);
