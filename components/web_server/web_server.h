/*
 * Web Server Component for ESP32-C6
 */

#pragma once

#include <esp_err.h>

 typedef struct {
    float temperature;
    float relative_humidity;
    float absolute_humidity;
    float pressure;
} sensor_data_t;

esp_err_t web_server_init(const char *ssid, const char *password, sensor_data_t *sd);
void web_server_get_sensors(sensor_data_t *out);
void web_server_set_sensors(const sensor_data_t *sd);
