/*
 * Display Driver Implementation for Waveshare ESP32-C6-LCD-1.47
 * ST7789 LCD controller via SPI
 */

#include "display_driver.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "display_driver";
static spi_device_handle_t spi_handle;

static esp_err_t lcd_spi_write_command(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };
    spi_device_transmit(spi_handle, &t);
    return ESP_OK;
}

static esp_err_t lcd_spi_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data
    };
    return spi_device_transmit(spi_handle, &t);
}

static esp_err_t lcd_init_commands(void)
{
    // ST7789 Initialization Sequence
    lcd_spi_write_command(0x11); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_spi_write_command(0x36); // Memory Access Control
    uint8_t mac = 0x00; // RGB mode, column address order
    lcd_spi_write_data(&mac, 1);

    lcd_spi_write_command(0x3A); // Pixel Format Set
    uint8_t pixfmt = 0x55; // 16-bit/pixel
    lcd_spi_write_data(&pixfmt, 1);

    lcd_spi_write_command(0x2A); // Column Address Set
    uint8_t col_start[4] = {0x00, 0x00, 0x00, 0xAF}; // 0-172
    lcd_spi_write_data(col_start, 4);

    lcd_spi_write_command(0x2B); // Row Address Set
    uint8_t row_start[4] = {0x00, 0x00, 0x01, 0x3F}; // 0-320
    lcd_spi_write_data(row_start, 4);

    lcd_spi_write_command(0x21); // Display Inversion On
    lcd_spi_write_command(0x13); // Normal Display Mode On

    lcd_spi_write_command(0x29); // Display On
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus for LCD");

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 20 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST_SPI, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 40 * 1000 * 1000, // 40 MHz
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST_SPI, &dev_cfg, &spi_handle));

    // Initialize GPIO for DC, RST, BL
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_RST) | (1ULL << LCD_PIN_BL),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

    // Reset LCD
    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize LCD commands
    lcd_init_commands();

    // Set backlight to 50%
    display_backlight_set(LCD_BACKLIGHT_BRIGHTNESS);

    ESP_LOGI(TAG, "LCD initialized successfully");
    return ESP_OK;
}

esp_err_t display_backlight_set(uint8_t brightness)
{
    // Use LEDC for PWM control of backlight
    static bool ledc_initialized = false;
    static ledc_channel_config_t ledc_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };

    if (!ledc_initialized) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));
        ledc_initialized = true;
    }

    ledc_conf.duty = brightness;
    return ledc_channel_config(&ledc_conf);
}

void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Set column and row addresses
    uint8_t col_addr[4] = {
        area->x1 >> 8, area->x1 & 0xFF,
        area->x2 >> 8, area->x2 & 0xFF
    };
    uint8_t row_addr[4] = {
        area->y1 >> 8, area->y1 & 0xFF,
        area->y2 >> 8, area->y2 & 0xFF
    };

    // DC high for data, low for command
    gpio_set_level(LCD_PIN_DC, 0);
    lcd_spi_write_command(0x2C); // Memory Write

    // Send pixel data
    t.length = lv_display_get_flush_size(disp) * 16;
    t.tx_buffer = px_map;
    t.user = (void*)1; // DC high during transfer
    spi_device_polling_transmit(spi_handle, &t);

    lv_display_flush_ready(disp);
}
