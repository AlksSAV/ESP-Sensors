/*
 * Waveshare ESP32-C6-LCD-1.47 + SHT45 + BMP581 Sensor Display
 * Main application file
 * 
 * Hardware connections:
 * LCD (ST7789):
 *   MOSI: GPIO6, SCLK: GPIO7, CS: GPIO14, DC: GPIO15, RST: GPIO21, BL: GPIO22
 * SHT45 (I2C):
 *   SDA: GPIO4, SCL: GPIO5
 * BMP581 (SPI):
 *   CS: GPIO3, MISO: GPIO8, SCK: GPIO9
 */

#include <esp_log.h>
#include "display_driver.h"
#include "sht45_driver.h"
#include "bmp581_driver.h"
#include "lvgl_port.h"

static const char *TAG = "main";

// Update interval in milliseconds
#define SENSOR_UPDATE_INTERVAL 2000

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Waveshare ESP32-C6-LCD-1.47 Sensor Display");

    // 1. Initialize LVGL (includes display initialization)
    if (lvgl_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return;
    }

    // 2. Initialize SHT45 sensor (I2C on GPIO4/5)
    if (sht45_init(4, 5) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHT45");
    }

    // 3. Initialize BMP581 sensor (SPI on GPIO3/8/9)
    if (bmp581_init(3) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BMP581");
    }

    // 4. Create UI
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    // Title
    lv_obj_t *title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "Sensor Data");
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, lv_font_default(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Temperature
    lv_obj_t *temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temp: --.-°C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 10, 60);

    // Humidity
    lv_obj_t *humid_label = lv_label_create(scr);
    lv_label_set_text(humid_label, "Humidity: --.-%");
    lv_obj_set_style_text_color(humid_label, lv_color_hex(0x0099FF), LV_PART_MAIN);
    lv_obj_align(humid_label, LV_ALIGN_TOP_LEFT, 10, 100);

    // Pressure
    lv_obj_t *press_label = lv_label_create(scr);
    lv_label_set_text(press_label, "Pressure: ---.- hPa");
    lv_obj_set_style_text_color(press_label, lv_color_hex(0xFF9900), LV_PART_MAIN);
    lv_obj_align(press_label, LV_ALIGN_TOP_LEFT, 10, 140);

    ESP_LOGI(TAG, "UI initialized, sensors starting");

    // 5. Main loop - read sensors and update display
    while (1) {
        sht45_data_t sht_data;
        bmp581_data_t bmp_data;
        char buf[64];

        // Read and display sensors
        if (sht45_read(&sht_data) == ESP_OK) {
            snprintf(buf, sizeof(buf), "Temp: %.1f°C", sht_data.temperature);
            lv_label_set_text(temp_label, buf);
            
            snprintf(buf, sizeof(buf), "Humidity: %.1f%%", sht_data.humidity);
            lv_label_set_text(humid_label, buf);
        }

        if (bmp581_read(&bmp_data) == ESP_OK) {
            snprintf(buf, sizeof(buf), "Pressure: %.1f hPa", bmp_data.pressure);
            lv_label_set_text(press_label, buf);
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL));
    }
}
