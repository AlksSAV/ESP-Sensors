/*
 * SHT45 Temperature and Humidity Sensor Driver
 * I2C interface
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

#define SHT45_ADDR 0x44  // Default I2C address

typedef struct {
    float temperature;    // Celsius
    float humidity;       // %RH
} sht45_data_t;

esp_err_t sht45_init(int sda_pin, int scl_pin);
esp_err_t sht45_read(sht45_data_t *data);
