/*
 * LVGL Port for ESP-IDF
 * Integration layer between LVGL and ESP-IDF
 */

#pragma once

#include <lvgl.h>
#include <esp_err.h>

esp_err_t lvgl_port_init(void);
void lvgl_port_task(void *arg);
