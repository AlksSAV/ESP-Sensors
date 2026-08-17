/*
 * Display Driver Implementation for Waveshare ESP32-C6-LCD-1.47
 * ST7789 LCD controller via SPI
 * LVGL v8 API
 */

#include "display_driver.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "display_driver";
static spi_device_handle_t spi_handle;

/* ---------- Низкоуровневые функции SPI ---------- */

static void lcd_dc_control(bool data)
{
    gpio_set_level((gpio_num_t)LCD_PIN_DC, data ? 1 : 0);
}

static esp_err_t lcd_spi_write_command(uint8_t cmd)
{
    lcd_dc_control(false);  /* DC = 0 — команда */
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    return spi_device_transmit(spi_handle, &t);
}

static esp_err_t lcd_spi_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    lcd_dc_control(true);   /* DC = 1 — данные */
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_transmit(spi_handle, &t);
}

/* ---------- Инициализация ST7789 ---------- */

static esp_err_t lcd_init_commands(void)
{
    /* Sleep Out */
    lcd_spi_write_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* Memory Access Control — RGB, слева-направо, сверху-вниз */
    lcd_spi_write_command(0x36);
    uint8_t mac = 0x00;
    lcd_spi_write_data(&mac, 1);

    /* Pixel Format Set — 16 bit */
    lcd_spi_write_command(0x3A);
    uint8_t pixfmt = 0x55;
    lcd_spi_write_data(&pixfmt, 1);

    /* Column Address Set: 0..171 (172 px) */
    lcd_spi_write_command(0x2A);
    uint8_t col[4] = { 0x00, 0x00, 0x00, 0xAB };
    lcd_spi_write_data(col, 4);

    /* Row Address Set: 0..319 (320 px) */
    lcd_spi_write_command(0x2B);
    uint8_t row[4] = { 0x00, 0x00, 0x01, 0x3F };
    lcd_spi_write_data(row, 4);

    /* Display Inversion On */
    lcd_spi_write_command(0x21);
    /* Normal Display Mode On */
    lcd_spi_write_command(0x13);

    /* Display On */
    lcd_spi_write_command(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

/* ---------- Публичный API ---------- */

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus for LCD");

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = (gpio_num_t)LCD_PIN_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = (gpio_num_t)LCD_PIN_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST_SPI, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;  /* 40 MHz */
    dev_cfg.spics_io_num = (gpio_num_t)LCD_PIN_CS;
    dev_cfg.queue_size = 7;
    dev_cfg.flags = SPI_DEVICE_HALFDUPLEX;
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST_SPI, &dev_cfg, &spi_handle));

    /* GPIO: DC, RST, BL */
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_RST) | (1ULL << LCD_PIN_BL);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* Сброс дисплея */
    gpio_set_level((gpio_num_t)LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Инициализация контроллера */
    lcd_init_commands();

    /* Яркость 50% */
    display_backlight_set(LCD_BACKLIGHT_BRIGHTNESS);

    ESP_LOGI(TAG, "LCD initialized successfully");
    return ESP_OK;
}

esp_err_t display_backlight_set(uint8_t brightness)
{
    static bool ledc_initialized = false;
    static ledc_channel_config_t ledc_conf = {
        .gpio_num   = (gpio_num_t)LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };

    if (!ledc_initialized) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode      = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num       = LEDC_TIMER_0,
            .freq_hz         = 5000,
            .clk_cfg         = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));
        ledc_initialized = true;
    }

    ledc_conf.duty = brightness;
    return ledc_channel_config(&ledc_conf);
}

/* ---------- Flush callback для LVGL v8 ---------- */

void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    size_t size = w * h * sizeof(lv_color_t);

    /* Column Address Set */
    lcd_spi_write_command(0x2A);
    uint8_t col[4] = {
        (uint8_t)(area->x1 >> 8), (uint8_t)(area->x1 & 0xFF),
        (uint8_t)(area->x2 >> 8), (uint8_t)(area->x2 & 0xFF),
    };
    lcd_spi_write_data(col, 4);

    /* Row Address Set */
    lcd_spi_write_command(0x2B);
    uint8_t row[4] = {
        (uint8_t)(area->y1 >> 8), (uint8_t)(area->y1 & 0xFF),
        (uint8_t)(area->y2 >> 8), (uint8_t)(area->y2 & 0xFF),
    };
    lcd_spi_write_data(row, 4);

    /* Memory Write */
    lcd_spi_write_command(0x2C);
    lcd_dc_control(true);

    /* Отправляем пиксели одним транзактом */
    spi_transaction_t t = {
        .length = size * 8,
        .tx_buffer = (uint8_t *)color_p,
    };
    spi_device_polling_transmit(spi_handle, &t);

    /* Готово — LVGL может рендерить следующую область */
    lv_disp_flush_ready(drv);
}
