#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo LCD + LVGL display, trả về lv_display_t
lv_display_t *ili9341_lcd_init(void);

#ifdef __cplusplus
}
#endif
