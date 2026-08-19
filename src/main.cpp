/*
 * Waveshare ESP32-C6-LCD-1.47 Sensor Display Demo
 * 
 * Отображает на дисплее:
 *   - Температура (демо-значение, 26°C)
 *   - Относительная влажность (демо-значение, 45%)
 *   - Абсолютная влажность (расчётная по формуле)
 *   - Давление (демо-значение, 1013.25 hPa)
 *
 * Веб-сервер на порту 80:
 *   / — HTML страница с данными
 *   /api/sensors — JSON API
 *
 * Когда подключишь SHT45 и BMP581 — заменишь демо-данные на реальные.
 */

#include <esp_log.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "display_driver.h"
#include "lvgl_port.h"
#include "web_server.h"

/* Декларация используемых шрифтов (LVGL v8 требует LV_FONT_DECLARE) */
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

static const char *TAG = "main";

#define UPDATE_INTERVAL_MS 2000

/* Общие данные сенсоров (для дисплея и веб-сервера) */
static sensor_data_t sensor_data = {
    .temperature = 26.0f,
    .relative_humidity = 45.0f,
    .absolute_humidity = 0.0f,
    .pressure = 1013.25f
};

/* Указатели на метки (для обновления из таймера) */
static lv_obj_t *t_label  = NULL;
static lv_obj_t *h_label  = NULL;
static lv_obj_t *ah_label = NULL;
static lv_obj_t *p_label  = NULL;

/*
 * Расчёт абсолютной влажности (г/м³):
 *   AH = (2.1674 * e) / (273.15 + T)
 *   e = RH/100 * 6.112 * exp(17.62*T / (243.12 + T))
 */
static float calc_absolute_humidity(float temp_c, float rel_humidity)
{
    float e = (rel_humidity / 100.0f) * 6.112f *
              expf(17.62f * temp_c / (243.12f + temp_c));
    return (2.1674f * e) / (273.15f + temp_c);
}

/* Callback таймера LVGL — обновляет данные каждые 2 секунды */
static void sensor_timer_cb(lv_timer_t *t)
{
    (void)t;
    char buf[48];

    /* Обновляем общие данные */
    sensor_data.absolute_humidity = calc_absolute_humidity(
        sensor_data.temperature,
        sensor_data.relative_humidity
    );

    /* Дисплей */
    snprintf(buf, sizeof(buf), "T: %+.1f C", sensor_data.temperature);
    lv_label_set_text(t_label, buf);

    snprintf(buf, sizeof(buf), "RH: %.1f %%", sensor_data.relative_humidity);
    lv_label_set_text(h_label, buf);

    snprintf(buf, sizeof(buf), "AH: %.2f g/m3", sensor_data.absolute_humidity);
    lv_label_set_text(ah_label, buf);

    snprintf(buf, sizeof(buf), "P: %.1f hPa", sensor_data.pressure);
    lv_label_set_text(p_label, buf);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 Sensor Display Demo ===");

    /* 1. Дисплей + LVGL (без запуска задачи!) */
    if (lvgl_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "LVGL init failed");
        return;
    }

    /* 2. Создаём UI ПОСЛЕ инициализации, ДО запуска lv_timer_handler */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Заголовок */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP32-C6 Monitor");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* Разделитель */
    lv_obj_t *sep = lv_line_create(scr);
    static lv_point_t pts[] = { {5, 34}, {LCD_WIDTH - 5, 34} };
    lv_line_set_points(sep, pts, 2);
    lv_obj_set_style_line_color(sep, lv_color_hex(0x333355), LV_PART_MAIN);
    lv_obj_set_style_line_width(sep, 1, LV_PART_MAIN);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 36);

    /* Строка 1 — Температура */
    t_label = lv_label_create(scr);
    lv_label_set_text(t_label, "T: --.- C");
    lv_obj_set_style_text_color(t_label, lv_color_hex(0xFF7777), LV_PART_MAIN);
    lv_obj_set_style_text_font(t_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(t_label, LV_ALIGN_TOP_LEFT, 12, 52);

    /* Строка 2 — Относительная влажность */
    h_label = lv_label_create(scr);
    lv_label_set_text(h_label, "RH: --.- %");
    lv_obj_set_style_text_color(h_label, lv_color_hex(0x77AAFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(h_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(h_label, LV_ALIGN_TOP_LEFT, 12, 88);

    /* Строка 3 — Абсолютная влажность */
    ah_label = lv_label_create(scr);
    lv_label_set_text(ah_label, "AH: --.- g/m3");
    lv_obj_set_style_text_color(ah_label, lv_color_hex(0x77DDAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(ah_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(ah_label, LV_ALIGN_TOP_LEFT, 12, 124);

    /* Строка 4 — Давление */
    p_label = lv_label_create(scr);
    lv_label_set_text(p_label, "P: ---.- hPa");
    lv_obj_set_style_text_color(p_label, lv_color_hex(0xFFBB66), LV_PART_MAIN);
    lv_obj_set_style_text_font(p_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(p_label, LV_ALIGN_TOP_LEFT, 12, 160);

    /* Подпись внизу */
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "[demo mode]");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* 3. Таймер LVGL для обновления данных (без гонки — в контексте LVGL) */
    lv_timer_create(sensor_timer_cb, UPDATE_INTERVAL_MS, NULL);

    ESP_LOGI(TAG, "UI ready. Starting LVGL loop");

    /* 4. Инициализация веб-сервера (Wi-Fi + HTTP) */
    ESP_LOGI(TAG, "Starting web server...");
    web_server_init(NULL, NULL, &sensor_data);
    ESP_LOGI(TAG, "Web server ready. Check http://<IP>/");

    /* 5. Главный цикл — lv_timer_handler в той же задаче */
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
