/*
 * LVGL Port Implementation for ESP-IDF
 * Handles LVGL initialization and FreeRTOS task
 * LVGL v8 API
 */

#include "lvgl_port.h"
#include "display_driver.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "lvgl_port";

/* Draw buffer — два буфера по 172x40 (RGB565, 16 bit) */
#define DRAW_BUF_WIDTH  LCD_WIDTH
#define DRAW_BUF_HEIGHT 40
#define DRAW_BUF_SIZE   (DRAW_BUF_WIDTH * DRAW_BUF_HEIGHT)

static lv_color_t *draw_buf1 = NULL;
static lv_color_t *draw_buf2 = NULL;

esp_err_t lvgl_port_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL port");

    /* 1. Инициализация дисплея */
    if (display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return ESP_FAIL;
    }

    /* 2. Инициализация LVGL */
    lv_init();

    /* 3. Выделяем draw buffers (DMA-совместимая память) */
    draw_buf1 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    draw_buf2 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (draw_buf1 == NULL || draw_buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate draw buffers");
        return ESP_FAIL;
    }

    /* 4. Регистрируем дисплей в LVGL */
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, draw_buf1, draw_buf2, DRAW_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "LVGL port initialized (%dx%d)", LCD_WIDTH, LCD_HEIGHT);
    return ESP_OK;
}

void lvgl_port_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "LVGL task started");

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
