/*
 * Display Driver Implementation for Waveshare ESP32-C6-LCD-1.47
 * ST7789 LCD controller via SPI
 * LVGL v8 API
 */

#include "display_driver.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
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
    return spi_device_polling_transmit(spi_handle, &t);
}

static esp_err_t lcd_spi_write_data(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    lcd_dc_control(true);   /* DC = 1 — данные */
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_polling_transmit(spi_handle, &t);
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

    /* Pixel Format Set — 16 bit (RGB565) */
    lcd_spi_write_command(0x3A);
    uint8_t pixfmt = 0x05;
    lcd_spi_write_data(&pixfmt, 1);

    /* --- Полная инициализация ST7789 (по Waveshare) --- */
    uint8_t d;
    lcd_spi_write_command(0xB0); d = 0x00; lcd_spi_write_data(&d, 1); d = 0xE8; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xB2); uint8_t b2[5] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; lcd_spi_write_data(b2, 5);
    lcd_spi_write_command(0xB7); d = 0x35; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xBB); d = 0x35; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xC0); d = 0x2C; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xC2); d = 0x01; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xC3); d = 0x13; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xC4); d = 0x20; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xC6); d = 0x0F; lcd_spi_write_data(&d, 1);
    lcd_spi_write_command(0xD0); uint8_t d0[2] = {0xA4, 0xA1}; lcd_spi_write_data(d0, 2);
    lcd_spi_write_command(0xD6); d = 0xA1; lcd_spi_write_data(&d, 1);

    /* Гамма-кривые */
    lcd_spi_write_command(0xE0);
    uint8_t e0[14] = {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30};
    lcd_spi_write_data(e0, 14);
    lcd_spi_write_command(0xE1);
    uint8_t e1[14] = {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32};
    lcd_spi_write_data(e1, 14);

    /* Display Inversion On */
    lcd_spi_write_command(0x21);
    /* Sleep Out */
    lcd_spi_write_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* Display On */
    lcd_spi_write_command(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

/* ---------- Публичный API ---------- */

/* Заливка экрана одним цветом (RGB565) ПОЛОСАМИ по 40 строк,
 * чтобы не превышать аппаратный лимит SPI (16KB) */
static void lcd_fill_screen(uint16_t color)
{
    size_t stripe_w = LCD_WIDTH;          /* 172 */
    size_t stripe_h = 40;                 /* строк за раз */
    size_t px = stripe_w * stripe_h;      /* 6880 пикселей */
    uint16_t *buf = (uint16_t *)heap_caps_malloc(px * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "fill: no DMA mem");
        return;
    }
    for (size_t i = 0; i < px; i++) {
        buf[i] = color;
    }

    for (uint16_t y = 0; y < LCD_HEIGHT; y += stripe_h) {
        uint16_t h = (y + stripe_h <= LCD_HEIGHT) ? stripe_h : (LCD_HEIGHT - y);
        uint16_t x1 = LCD_OFFSET_X;
        uint16_t x2 = LCD_OFFSET_X + LCD_WIDTH - 1;
        uint16_t y1 = LCD_OFFSET_Y + y;
        uint16_t y2 = LCD_OFFSET_Y + y + h - 1;

        lcd_spi_write_command(0x2A);
        uint8_t col[4] = { (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF), (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF) };
        lcd_spi_write_data(col, 4);

        lcd_spi_write_command(0x2B);
        uint8_t row[4] = { (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF), (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF) };
        lcd_spi_write_data(row, 4);

        lcd_spi_write_command(0x2C);
        lcd_dc_control(true);

        spi_transaction_t t = {
            .length = stripe_w * h * 16,
            .tx_buffer = buf,
        };
        esp_err_t e = spi_device_polling_transmit(spi_handle, &t);
        if (e != ESP_OK) ESP_LOGE(TAG, "fill stripe y=%u err=%d", y, e);
    }
    heap_caps_free(buf);
}

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus for LCD");

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = (gpio_num_t)LCD_PIN_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = (gpio_num_t)LCD_PIN_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = LCD_WIDTH * 40 * sizeof(uint16_t); /* 172*40*2 = 13760 байт */
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST_SPI, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.mode = 0;
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;  /* 40 MHz */
    dev_cfg.spics_io_num = (gpio_num_t)LCD_PIN_CS;
    dev_cfg.queue_size = 7;
    dev_cfg.flags = SPI_DEVICE_HALFDUPLEX;
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST_SPI, &dev_cfg, &spi_handle));

    /* GPIO: DC, RST (BL управляется через LEDC отдельно) */
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_RST);
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
        .duty       = brightness,
        .hpoint     = 0,
        /* .flags.output_invert = 0 */
    };

    if (!ledc_initialized) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode      = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num       = LEDC_TIMER_0,
            .freq_hz         = 1000,
            .clk_cfg         = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));
        ledc_initialized = true;
    }

    /* Обновляем duty без повторного конфигурирования пина */
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    return ESP_OK;
}

/* ---------- Flush callback для LVGL v8 ---------- */

void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    size_t size = w * h * sizeof(lv_color_t);

    uint16_t x1 = area->x1 + LCD_OFFSET_X;
    uint16_t x2 = area->x2 + LCD_OFFSET_X;
    uint16_t y1 = area->y1 + LCD_OFFSET_Y;
    uint16_t y2 = area->y2 + LCD_OFFSET_Y;

    /* Column Address Set (с учётом offset X) */
    lcd_spi_write_command(0x2A);
    uint8_t col[4] = {
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
        (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF),
    };
    lcd_spi_write_data(col, 4);

    /* Row Address Set */
    lcd_spi_write_command(0x2B);
    uint8_t row[4] = {
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
        (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF),
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

    lv_disp_flush_ready(drv);
}
