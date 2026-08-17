idf_component_register(
    SRCS "display_driver.cpp" "lvgl_port.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver heap lvgl
)