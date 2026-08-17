/*
 * SHT45 Temperature and Humidity Sensor Driver Implementation
 * I2C interface with measurement commands
 */

#include "sht45_driver.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "sht45";
static i2c_master_dev_handle_t i2c_dev_handle;

esp_err_t sht45_init(int sda_pin, int scl_pin)
{
    ESP_LOGI(TAG, "Initializing SHT45 on I2C");

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT45_ADDR,
        .scl_speed_hz = 400000,
        .scl_wait_us = 0,
        .flags.enable_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &i2c_dev_handle));

    ESP_LOGI(TAG, "SHT45 initialized successfully");
    return ESP_OK;
}

esp_err_t sht45_read(sht45_data_t *data)
{
    // Send heating command for measurement (no heating, high precision)
    uint8_t cmd[] = {0xFD}; // Precision measurement, clock stretch enabled
    i2c_master_transmit(i2c_dev_handle, cmd, 1, 100);

    // Wait for measurement to complete (~12ms for high precision)
    vTaskDelay(pdMS_TO_TICKS(15));

    // Read 6 bytes (humidity MSB, LSB, temp MSB, LSB, CRC, CRC)
    uint8_t raw_data[6];
    i2c_master_receive(i2c_dev_handle, raw_data, 6, 100);

    // Calculate humidity (%RH)
    int32_t humidity_raw = ((int32_t)raw_data[0] << 8 | raw_data[1]);
    data->humidity = -6.0f + 125.0f * ((float)humidity_raw / 65536.0f);

    // Calculate temperature (°C)
    int32_t temp_raw = ((int32_t)raw_data[3] << 8 | raw_data[4]);
    data->temperature = -45.0f + 175.0f * ((float)temp_raw / 65536.0f);

    ESP_LOGI(TAG, "SHT45: Temp=%.2f°C, Humidity=%.2f%%", data->temperature, data->humidity);
    return ESP_OK;
}
