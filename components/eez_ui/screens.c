#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

/* =========================
 *  MAIN SCREEN
 * ========================= */

void create_screen_main(void) {
    /* Root screen */
    lv_obj_t *root = lv_obj_create(NULL);
    objects.main = root;
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, 320, 240);

    /* NỀN TRẮNG HOÀN TOÀN */
    lv_obj_set_style_bg_color(root, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(root, 0,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *parent_obj = root;

    /* ---------- HEADER CYAN ---------- */
    {
        lv_obj_t *obj = lv_obj_create(parent_obj);
        objects.obj0 = obj;
        lv_obj_set_pos(obj, 0, 0);
        lv_obj_set_size(obj, 320, 40);

        /* Cyan #00BFE8 */
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x00BFE8),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(obj, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

        /* Viền dưới xám rất nhạt */
        lv_obj_set_style_border_color(obj, lv_color_hex(0xE0E0E0),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 2,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- label_header: "Heart Rate" ---------- */
    {
        lv_obj_t *obj = lv_label_create(parent_obj);
        objects.label_header = obj;
        lv_obj_set_pos(obj, 4, 10);
        lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_text(obj, "Heart Rate");
        lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(obj, &lv_font_montserrat_18,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        /* Chữ đen, full opacity */
        lv_obj_set_style_text_color(obj, lv_color_hex(0x000000),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(obj, LV_OPA_COVER,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- CHART ECG ---------- */
    {
        lv_obj_t *obj = lv_chart_create(parent_obj);
        objects.obj1 = obj;
        lv_obj_set_pos(obj, 8, 56);
        lv_obj_set_size(obj, 192, 156);

        /* Nền chart TRẮNG 100% */
        lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_radius(obj, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 2,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        /* Viền xám nhẹ (không xanh) */
        lv_obj_set_style_border_color(obj, lv_color_hex(0xE0E0E0),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);

        /* Grid xám nhạt */
        lv_chart_set_div_line_count(obj, 4, 4);
        lv_obj_set_style_line_color(obj, lv_color_hex(0xE0E0E0),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_opa(obj, LV_OPA_60,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- CARD BPM BÊN PHẢI ---------- */
    {
        lv_obj_t *obj = lv_obj_create(parent_obj);
        lv_obj_set_pos(obj, 210, 56);
        lv_obj_set_size(obj, 96, 88);

        /* Card nền TRẮNG 100% */
        lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(obj, 12,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 2,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xE0E0E0),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- label_bpm (số lớn) ---------- */
    {
        lv_obj_t *obj = lv_label_create(parent_obj);
        objects.label_bpm = obj;
        lv_obj_set_pos(obj, 218, 70);
        lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_text(obj, "000");
        lv_obj_set_style_text_font(obj, &lv_font_montserrat_36,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(obj, lv_color_hex(0x000000),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(obj, LV_OPA_COVER,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- label_beatsperminute (BPM) ---------- */
    {
        lv_obj_t *obj = lv_label_create(parent_obj);
        objects.label_beatsperminute = obj;
        lv_obj_set_pos(obj, 235, 110);
        lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_text(obj, "BPM");
        lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(obj, lv_color_hex(0x888888),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(obj, LV_OPA_COVER,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- ICON TRÁI TIM ---------- */
    {
        lv_obj_t *obj = lv_image_create(parent_obj);
        lv_obj_set_pos(obj, 225, 152);
        lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_image_set_src(obj, &img_heart);
    }

    tick_screen_main();
}

void tick_screen_main(void) {
}

/* =========================
 *  SCREEN MANAGEMENT
 * ========================= */

typedef void (*tick_screen_func_t)(void);

tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};

void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}

void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens(void) {
         lv_disp_t *dispp = lv_disp_get_default();
     lv_theme_t *theme = lv_theme_default_init(dispp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              false,
                                               LV_FONT_DEFAULT);
     lv_disp_set_theme(dispp, theme);

    create_screen_main();
}
