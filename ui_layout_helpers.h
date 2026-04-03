#pragma once
/* ui_layout_helpers.h - Layout responsive pour 480×320 ET 1024×600
 * SX(100) sur TFT = 100px
 * SX(100) sur WS7B = 213px (×2.133)
 */
#include "lvgl.h"
#include "display_config.h"

static inline lv_obj_t* ui_create_card(lv_obj_t* p, lv_coord_t w, lv_coord_t h,
                                     lv_coord_t x=0, lv_coord_t y=0){
    lv_obj_t* c=lv_obj_create(p);
    lv_obj_set_size(c, SX(w), SY(h));
    lv_obj_set_pos(c, SX(x), SY(y));
    lv_obj_add_style(c, &ui.style_card, 0);
    return c;
}

static inline lv_obj_t* ui_title_label(lv_obj_t* p, const char* t){
    lv_obj_t* l=lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_FONT_SIZE_TITLE, 0);
    return l;
}

static inline lv_obj_t* ui_value_label(lv_obj_t* p, const char* t){
    lv_obj_t* l=lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_FONT_SIZE_VALUE, 0);
    return l;
}

static inline lv_obj_t* ui_unit_label(lv_obj_t* p, const char* t){
    lv_obj_t* l=lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_FONT_SIZE_UNIT, 0);
    lv_obj_set_style_text_color(l, COLOR_GRAY, 0);
    return l;
}

static inline lv_obj_t* ui_small_label(lv_obj_t* p, const char* t){
    lv_obj_t* l=lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_FONT_SIZE_SMALL, 0);
    return l;
}

static inline lv_obj_t* ui_status_bar_create(lv_obj_t* scr){
    lv_obj_t* sb=lv_obj_create(scr);
    lv_obj_set_size(sb, SCREEN_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x090D12), 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
    return sb;
}