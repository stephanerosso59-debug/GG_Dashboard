/*
 * ui_helpers.cpp - Widgets LVGL communs
 */
#include "ui_helpers.h"
#include "../config.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ui_create_back_btn()  — conservé pour compatibilité éventuelle
// ─────────────────────────────────────────────────────────────────────────────
lv_obj_t* ui_create_back_btn(lv_obj_t* parent, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 70, 36);
    lv_obj_set_pos(btn, SCREEN_WIDTH - 82, 8);
    lv_obj_set_style_bg_color(btn, COLOR_BG_CARD, 0);
    lv_obj_set_style_border_color(btn, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Retour");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_obj_center(lbl);
    return btn;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ui_create_title_bar()  — titre seul (plus de bouton retour)
// ─────────────────────────────────────────────────────────────────────────────
lv_obj_t* ui_create_title_bar(lv_obj_t* parent, const char* icon, const char* text,
                               lv_color_t color) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  %s", icon, text);
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, 12, 28);   // y=28 car status bar globale prend y=0..21
    return lbl;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ui_create_nav_bar()  — barre navigation inférieure (inspirée du code référence)
// ─────────────────────────────────────────────────────────────────────────────
// Callback statique partagé : lit la page cible depuis user_data du bouton
static void _nav_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    DashPage p = (DashPage)(intptr_t)lv_obj_get_user_data(btn);
    ui.showPage(p);
}

void ui_create_nav_bar(lv_obj_t* parent, DashPage active_page) {
    static const struct {
        const char* icon;
        const char* text;
        DashPage    page;
    } items[] = {
        { LV_SYMBOL_HOME,         "Accueil",   PAGE_HOME    },
        { LV_SYMBOL_LOOP,         "Lumieres",  PAGE_LIGHTS  },
        { LV_SYMBOL_CHARGE,       "Energie",   PAGE_BATTERY },
        { LV_SYMBOL_WARNING,      "Chauffage", PAGE_HEATING },
        { LV_SYMBOL_SETTINGS,     "Systeme",   PAGE_SYSTEM  },
    };
    const int N = 5;

    // Fond de la barre
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, SCREEN_WIDTH, 56);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 2, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    int btn_w = (SCREEN_WIDTH - 10) / N;   // (480-10)/5 = 94px
    for (int i = 0; i < N; i++) {
        bool active = (items[i].page == active_page);
        lv_obj_t* btn = lv_btn_create(bar);
        lv_obj_set_size(btn, btn_w, 50);
        lv_obj_set_pos(btn, 2 + i * (btn_w + 1), 2);
        lv_obj_add_style(btn, active ? &ui.style_nav_btn_active : &ui.style_nav_btn, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)items[i].page);
        lv_obj_add_event_cb(btn, _nav_btn_cb, LV_EVENT_CLICKED, nullptr);

        // Icône (en haut, centré)
        lv_obj_t* lbl_ic = lv_label_create(btn);
        lv_label_set_text(lbl_ic, items[i].icon);
        lv_obj_set_style_text_font(lbl_ic, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_ic, active ? COLOR_BLUE : COLOR_GRAY, 0);
        lv_obj_align(lbl_ic, LV_ALIGN_TOP_MID, 0, 2);

        // Label texte (en bas, centré)
        lv_obj_t* lbl_t = lv_label_create(btn);
        lv_label_set_text(lbl_t, items[i].text);
        lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(lbl_t, active ? COLOR_BLUE : COLOR_GRAY, 0);
        lv_obj_align(lbl_t, LV_ALIGN_BOTTOM_MID, 0, -2);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  ui_make_val_row()  — ligne icone + valeur + unité
// ─────────────────────────────────────────────────────────────────────────────
lv_obj_t* ui_make_val_row(lv_obj_t* parent, const char* icon, const char* unit,
                           int py, lv_color_t color) {
    lv_obj_t* lbl_ic = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_ic, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_ic, COLOR_GRAY, 0);
    lv_label_set_text(lbl_ic, icon);
    lv_obj_set_pos(lbl_ic, 4, py);
    lv_obj_t* lbl_val = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_val, color, 0);
    lv_label_set_text(lbl_val, "--");
    lv_obj_set_pos(lbl_val, 28, py - 1);
    lv_obj_t* lbl_u = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_u, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_u, COLOR_GRAY, 0);
    lv_label_set_text(lbl_u, unit);
    lv_obj_set_pos(lbl_u, 90, py);
    return lbl_val;
}
