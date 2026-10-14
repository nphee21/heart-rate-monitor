#include "ili9341_lcd.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_heap_caps.h"

static const char *TAG = "ili9341_lcd";

/*=====================================================================*/
/*                          CẤU HÌNH PHẦN CỨNG                          */
/*=====================================================================*/

/* Độ phân giải sau khi xoay ngang */
#define LCD_HOR_RES   320
#define LCD_VER_RES   240

/* SPI host cho LCD */
#define LCD_SPI_HOST  SPI2_HOST

/* Chân SPI màn hình (sửa theo wiring của bạn nếu khác) */
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    15
#define PIN_NUM_DC     2
#define PIN_NUM_RST    4
#define PIN_NUM_BCKL  -1   // backlight

#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)

#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8

/* Mỗi lần flush ~40 dòng để tiết kiệm RAM */
#define FLUSH_LINES         40

static esp_lcd_panel_handle_t s_panel_handle = NULL;

static void ili9341_flush_cb(lv_display_t *disp,
                             const lv_area_t *area,
                             uint8_t *px_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;


    const void *color_buf = (const void *)px_map;

    esp_lcd_panel_draw_bitmap(s_panel_handle,
                              x1, y1,
                              x2 + 1, y2 + 1,
                              color_buf);

    lv_display_flush_ready(disp);
}

lv_display_t *ili9341_lcd_init(void)
{
    /* Backlight */
    gpio_config_t bk_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL,
        .pull_down_en = 0,
        .pull_up_en   = 0,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&bk_cfg);
    gpio_set_level(PIN_NUM_BCKL, 1);  // bật sáng

    /* SPI BUS */
    spi_bus_config_t buscfg = {
        .sclk_io_num     = PIN_NUM_CLK,
        .mosi_io_num     = PIN_NUM_MOSI,
        .miso_io_num     = PIN_NUM_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_HOR_RES * FLUSH_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* PANEL IO */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = PIN_NUM_DC,
        .cs_gpio_num       = PIN_NUM_CS,
        .pclk_hz           = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = LCD_CMD_BITS,
        .lcd_param_bits    = LCD_PARAM_BITS,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &io_handle));

    /* PANEL ILI9341*/
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order  = ESP_LCD_COLOR_SPACE_RGB, 
        .bits_per_pixel = 16,
    };

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config,
                                              &s_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

    /* Xoay ngang: 320x240 */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    /* LVGL DISPLAY */
    lv_display_t *disp = lv_display_create(LCD_HOR_RES, LCD_VER_RES);

    /* Buffer cho 40 dòng, dùng 2 buffer (partial double buffer) */
    size_t buf_size_bytes = LCD_HOR_RES * FLUSH_LINES * sizeof(lv_color_t);

    lv_color_t *buf1 = heap_caps_malloc(buf_size_bytes,
                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *buf2 = heap_caps_malloc(buf_size_bytes,
                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1 && buf2);

    lv_display_set_flush_cb(disp, ili9341_flush_cb);

    lv_display_set_buffers(disp,
                           buf1,
                           buf2,
                           buf_size_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "ILI9341 + LVGL init OK (320x240 landscape)");
    return disp;
}
