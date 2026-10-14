idf_component_register(
    SRCS "ili9341_lcd.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lcd driver esp_lcd_ili9341
)
