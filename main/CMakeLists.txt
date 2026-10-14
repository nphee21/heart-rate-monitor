idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl ili9341_lcd eez_ui nvs_flash driver esp_timer
)
