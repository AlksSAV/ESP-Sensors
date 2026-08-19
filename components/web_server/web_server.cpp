/*
 * Web Server Implementation for ESP32-C6
 * HTTP server with sensor data endpoints
 */

#include "web_server.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <lwip/err.h>
#include <string.h>

static const char *TAG = "web_server";
#define WIFI_SSID       "Bertovic"
#define WIFI_PASS       "18072019"
#define MAX_RETRY       10

static sensor_data_t *g_sensor_data = NULL;
static httpd_handle_t http_server_handle = NULL;

static const char HTML_PAGE[] =
"<!DOCTYPE html>"
"<html lang=\"ru\">"
"<head>"
"  <meta charset=\"UTF-8\">"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"  <title>ESP32-C6 Monitor</title>"
"  <style>"
"    * { margin: 0; padding: 0; box-sizing: border-box; }"
"    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;"
"      background: #0a0a1a; color: #ffffff; display: flex;"
"      flex-direction: column; align-items: center; min-height: 100vh; padding: 20px; }"
"    h1 { font-size: 1.5rem; margin: 10px 0 5px; }"
"    .sensor-card { background: #1a1a2e; border-radius: 8px; padding: 20px;"
"      margin: 8px 0; width: 100%; max-width: 400px; border-left: 4px solid; }"
"    .sensor-card.temp   { border-left-color: #ff7777; }"
"    .sensor-card.hum    { border-left-color: #77aaff; }"
"    .sensor-card.ah     { border-left-color: #77ddaa; }"
"    .sensor-card.pres   { border-left-color: #ffbb66; }"
"    .sensor-label { font-size: 0.85rem; opacity: 0.7; margin-bottom: 5px; }"
"    .sensor-value { font-size: 2rem; font-weight: bold; }"
"    .sensor-unit  { font-size: 1rem; opacity: 0.6; }"
"    .status { margin-top: 20px; font-size: 0.8rem; opacity: 0.4; }"
"    #loading { opacity: 0.3; text-align: center; margin-top: 40px; }"
"  </style>"
"</head>"
"<body>"
"  <h1>ESP32-C6 Monitor</h1>"
"  <div id=\"loading\">Loading...</div>"
"  <div class=\"sensor-card temp\">"
"    <div class=\"sensor-label\">Temperature</div>"
"    <div class=\"sensor-value\" id=\"temp\">--.-</div>"
"    <span class=\"sensor-unit\">°C</span></div>"
"  <div class=\"sensor-card hum\">"
"    <div class=\"sensor-label\">Relative Humidity</div>"
"    <div class=\"sensor-value\" id=\"rh\">--.-</div>"
"    <span class=\"sensor-unit\">%RH</span></div>"
"  <div class=\"sensor-card ah\">"
"    <div class=\"sensor-label\">Absolute Humidity</div>"
"    <div class=\"sensor-value\" id=\"ah\">--.-</div>"
"    <span class=\"sensor-unit\">g/m³</span></div>"
"  <div class=\"sensor-card pres\">"
"    <div class=\"sensor-label\">Pressure</div>"
"    <div class=\"sensor-value\" id=\"pres\">---.-</div>"
"    <span class=\"sensor-unit\">hPa</span></div>"
"  <div class=\"status\">ESP32-C6 <span id=\"ip\">...</span></div>"
"  <script>"
"    function updateSensors() {"
"      fetch('/api/sensors').then(r => r.json()).then(data => {"
"        document.getElementById('temp').textContent = data.temp.toFixed(1);"
"        document.getElementById('rh').textContent = data.rh.toFixed(1);"
"        document.getElementById('ah').textContent = data.ah.toFixed(2);"
"        document.getElementById('pres').textContent = data.pres.toFixed(1);"
"      }).catch(() => { document.getElementById('temp').textContent = 'Err'; });"
"    }"
"    document.getElementById('ip').textContent = window.location.hostname;"
"    updateSensors(); setInterval(updateSensors, 2000);"
"  </script>"
"</body>"
"</html>";

static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, HTML_PAGE);
    return ESP_OK;
}

static esp_err_t sensors_handler(httpd_req_t *req) {
    if (g_sensor_data == NULL) { httpd_resp_send_500(req); return ESP_OK; }
    char json[128];
    snprintf(json, sizeof(json),
        "{\"temp\":%.1f,\"rh\":%.1f,\"ah\":%.2f,\"pres\":%.1f}",
        g_sensor_data->temperature, g_sensor_data->relative_humidity,
        g_sensor_data->absolute_humidity, g_sensor_data->pressure);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static const httpd_uri_t uris[] = {
    { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL },
    { .uri = "/api/sensors", .method = HTTP_GET, .handler = sensors_handler, .user_ctx = NULL }
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        char ip[16];
        esp_ip4addr_ntoa(&e->ip_info.ip, ip, sizeof(ip));
        ESP_LOGI(TAG, "Connected! IP: %s", ip);
    }
}

static esp_err_t wifi_init_sta(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    wifi_config_t wc = {};
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    int retry = 0;
    while (retry < MAX_RETRY) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip[16];
            esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));
            ESP_LOGI(TAG, "Connected to %s, IP: %s", WIFI_SSID, ip);
            return ESP_OK;
        }
        retry++;
        ESP_LOGI(TAG, "Connecting to %s... (%d/%d)", WIFI_SSID, retry, MAX_RETRY);
    }
    ESP_LOGE(TAG, "Failed to connect to %s", WIFI_SSID);
    return ESP_FAIL;
}

static esp_err_t http_server_start(sensor_data_t *sdata) {
    g_sensor_data = sdata;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 5;
    config.stack_size = 4096;
    config.max_uri_handlers = 4;
    ESP_ERROR_CHECK(httpd_start(&http_server_handle, &config));
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(http_server_handle, &uris[i]));
    }
    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t web_server_init(const char *ssid, const char *password, sensor_data_t *sensor_data) {
    ESP_LOGI(TAG, "Initializing web server");
    if (ssid != NULL && strlen(ssid) > 0) {
        strncpy((char *)WIFI_SSID, ssid, sizeof(WIFI_SSID) - 1);
    }
    if (password != NULL && strlen(password) > 0) {
        strncpy((char *)WIFI_PASS, password, sizeof(WIFI_PASS) - 1);
    }
    if (wifi_init_sta() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed, starting without network");
    }
    if (http_server_start(sensor_data) != ESP_OK) {
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    return ESP_OK;
}

void web_server_get_sensors(sensor_data_t *out) {
    if (g_sensor_data != NULL && out != NULL) {
        *out = *g_sensor_data;
    }
}