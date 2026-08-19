idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_http_server nvs_flash
)
