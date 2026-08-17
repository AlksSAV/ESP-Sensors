/*
 * LVGL Port Implementation for ESP-IDF
 * Handles LVGL initialization and FreeRTOS task
 */

#include "lvgl_port.h"
#include "display_driver.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "lvgl_port";

static esp_timer_handle_t lvgl_tick_timer = NULL;
static lv_display_t *disp = NULL;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

esp_err_t lvgl_port_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL port");

    // Initialize display
    if (display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return ESP_FAIL;
    }

    // Initialize LVGL core
    lv_init();

    // Create display driver
    disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp, display_flush_cb);

    // Create periodic timer for LVGL tick (1ms)
    esp_timer_create_args_t tick_timer_cfg = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = false,
        .is_oneshot = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_cfg, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1000)); // 1ms

    ESP_LOGI(TAG, "LVGL port initialized");
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
