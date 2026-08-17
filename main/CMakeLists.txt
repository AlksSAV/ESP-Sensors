idf_component_register(
    SRCS "main.cpp" "display_driver.cpp" "sht45_driver.cpp" "bmp581_driver.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver lvgl_port esp_timer
)
