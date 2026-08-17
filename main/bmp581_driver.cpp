/*
 * BMP581 Pressure and Temperature Sensor Driver Implementation
 * SPI interface with Bosch API
 */

#include "bmp581_driver.h"
#include <driver/spi_master.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "bmp581";
static spi_device_handle_t spi_handle;

// BMP581 registers
#define BMP581_ADDR_PIN_GND 0x76  // I2C address when ADDR pin is GND
#define BMP581_CHIP_ID_REG 0x00
#define BMP581_PRESSURE_DATA_REG 0x04
#define BMP581_TEMP_DATA_REG 0x07

esp_err_t bmp581_init(int cs_pin)
{
    ESP_LOGI(TAG, "Initializing BMP581 on SPI");

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = -1,  // BMP581 only uses MISO for reading
        .miso_io_num = 8,   // BMP581 MISO
        .sclk_io_num = 9,   // BMP581 SCK
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,  // 1 MHz (initial)
        .mode = 0,
        .spics_io_num = cs_pin,
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_handle));

    // Read chip ID to verify communication
    uint8_t cmd = 0xB0 | 0x80;  // Read command
    uint8_t chip_id = 0;
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.rx_buffer = &chip_id;
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));

    ESP_LOGI(TAG, "BMP581 Chip ID: 0x%02X", chip_id);
    
    if (chip_id != 0x50) {
        ESP_LOGE(TAG, "Invalid BMP581 chip ID! Expected 0x50, got 0x%02X", chip_id);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "BMP581 initialized successfully");
    return ESP_OK;
}

esp_err_t bmp581_read(bmp581_data_t *data)
{
    // Read pressure and temperature data (8 bytes: 3 bytes pressure MSB, 2 bytes pressure LSB, 
    // 3 bytes temp MSB, 2 bytes temp LSB - simplified reading)
    
    uint8_t cmd = BMP581_PRESSURE_DATA_REG | 0x80;  // Read command
    uint8_t raw_data[8] = {0};
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 8;  // Command byte
    t.tx_buffer = &cmd;
    t.rxlength = 8 * 8;  // 8 bytes response
    t.rx_buffer = &raw_data[1];  // Skip first byte (command echo)
    
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));

    // Parse pressure data (20-bit pressure value)
    uint32_t pressure_raw = ((uint32_t)raw_data[1] << 12) | 
                            ((uint32_t)raw_data[2] << 4) | 
                            ((raw_data[3] >> 4) & 0x0F);
    
    // Parse temperature data (20-bit temperature value)
    int32_t temp_raw = ((uint32_t)raw_data[5] << 12) | 
                       ((uint32_t)raw_data[6] << 4) | 
                       ((raw_data[7] >> 4) & 0x0F);

    // Convert to physical values (simplified calculation)
    // For accurate values, Bosch BMP581 API should be used
    data->pressure = (float)pressure_raw * 0.001f;  // Simplified: hPa
    data->temperature = (float)temp_raw * 0.001f - 45.0f;  // Simplified: °C

    ESP_LOGI(TAG, "BMP581: Pressure=%.2f hPa, Temp=%.2f°C", data->pressure, data->temperature);
    return ESP_OK;
}
