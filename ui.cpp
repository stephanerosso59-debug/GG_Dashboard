/*
 * ui.cpp - Gestionnaire principal LVGL
 */
#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"

UiManager ui;

// ============================================================
//  begin()
// ============================================================
void UiManager::begin() {
    _initStyles();
    _buildAllPages();
    showPage(PAGE_HOME);
}

// ============================================================
//  update() - A appeler dans loop()
// ============================================================
void UiManager::update() {
    lv_timer_handler();
    page_status_update();   // status bar globale — toujours actif
    switch (_currentPage) {
        case PAGE_HOME:    page_home_update();    break;
        case PAGE_BATTERY: page_battery_update(); break;
        case PAGE_HEATING: page_heating_update(); break;
        case PAGE_SYSTEM:  page_system_update();  break;
        default: break;
    }
}

// ============================================================
//  showPage()
// ============================================================
void UiManager::showPage(DashPage page) {
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (_pages[i]) lv_obj_add_flag(_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (_pages[page]) lv_obj_clear_flag(_pages[page], LV_OBJ_FLAG_HIDDEN);
    _currentPage = page;
}

// ============================================================
//  _initStyles()
// ============================================================
void UiManager::_initStyles() {
    // Style carte (fond semi-transparent)
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_BG_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_90);
    lv_style_set_border_color(&style_card, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_opa(&style_card, LV_OPA_50);
    lv_style_set_radius(&style_card, 14);
    lv_style_set_pad_all(&style_card, 8);
    lv_style_set_shadow_width(&style_card, 12);
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x0D1117));

    // Bouton navigation
    lv_style_init(&style_btn_nav);
    lv_style_set_bg_color(&style_btn_nav, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_btn_nav, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_nav, lv_color_black());
    lv_style_set_radius(&style_btn_nav, 10);
    lv_style_set_border_width(&style_btn_nav, 0);

    // Labels titre
    lv_style_init(&style_label_title);
    lv_style_set_text_color(&style_label_title, COLOR_ACCENT);
    lv_style_set_text_font(&style_label_title, &lv_font_montserrat_16);

    // Labels valeurs
    lv_style_init(&style_label_value);
    lv_style_set_text_color(&style_label_value, COLOR_WHITE);
    lv_style_set_text_font(&style_label_value, &lv_font_montserrat_28);

    // Labels unites
    lv_style_init(&style_label_unit);
    lv_style_set_text_color(&style_label_unit, COLOR_GRAY);
    lv_style_set_text_font(&style_label_unit, &lv_font_montserrat_14);

    // Bouton barre de navigation — inactif
    lv_style_init(&style_nav_btn);
    lv_style_set_bg_color(&style_nav_btn, lv_color_hex(0x21262D));
    lv_style_set_bg_opa(&style_nav_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_nav_btn, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_nav_btn, 1);
    lv_style_set_radius(&style_nav_btn, 6);
    lv_style_set_pad_all(&style_nav_btn, 2);

    // Bouton barre de navigation — page courante (actif)
    lv_style_init(&style_nav_btn_active);
    lv_style_set_bg_color(&style_nav_btn_active, lv_color_hex(0x1C2F47));
    lv_style_set_bg_opa(&style_nav_btn_active, LV_OPA_COVER);
    lv_style_set_border_color(&style_nav_btn_active, COLOR_BLUE);
    lv_style_set_border_width(&style_nav_btn_active, 1);
    lv_style_set_radius(&style_nav_btn_active, 6);
    lv_style_set_pad_all(&style_nav_btn_active, 2);
}

// ============================================================
//  ui_draw_bg() - Fond commun dashboard
// ============================================================
void ui_draw_bg(lv_obj_t* parent) {
    lv_obj_t* bg = lv_obj_create(parent);
    lv_obj_set_size(bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_grad_color(bg, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_bg_grad_dir(bg, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_set_style_radius(bg, 0, 0);
    for (int y = 80; y < SCREEN_HEIGHT; y += 80) {
        lv_obj_t* ln = lv_obj_create(parent);
        lv_obj_set_size(ln, SCREEN_WIDTH, 1);
        lv_obj_set_pos(ln, 0, y);
        lv_obj_set_style_bg_color(ln, lv_color_hex(0x21262D), 0);
        lv_obj_set_style_bg_opa(ln, LV_OPA_40, 0);
        lv_obj_set_style_border_width(ln, 0, 0);
        lv_obj_set_style_radius(ln, 0, 0);
    }
}

// ============================================================
//  _buildAllPages()
// ============================================================
void UiManager::_buildAllPages() {
    lv_obj_t* screen = lv_scr_act();

    for (int i = 0; i < PAGE_COUNT; i++) {
        _pages[i] = lv_obj_create(screen);
        lv_obj_set_size(_pages[i], SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_pos(_pages[i], 0, 0);
        lv_obj_set_style_bg_color(_pages[i], COLOR_BG_DARK, 0);
        lv_obj_set_style_bg_opa(_pages[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(_pages[i], 0, 0);
        lv_obj_set_style_radius(_pages[i], 0, 0);
        lv_obj_set_style_pad_all(_pages[i], 0, 0);
        lv_obj_add_flag(_pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    page_home_build(_pages[PAGE_HOME]);
    page_lights_build(_pages[PAGE_LIGHTS]);
    page_battery_build(_pages[PAGE_BATTERY]);
    page_heating_build(_pages[PAGE_HEATING]);
    page_system_build(_pages[PAGE_SYSTEM]);

    // ── Barre de statut globale (overlay — au-dessus de toutes les pages) ──────
    lv_obj_t* sb = lv_obj_create(screen);
    lv_obj_set_size(sb, SCREEN_WIDTH, 22);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x090D12), 0);
    lv_obj_set_style_bg_opa(sb, 210, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_hor(sb, 6, 0);
    lv_obj_set_style_pad_ver(sb, 2, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    // BLE
    lbl_sb_bt = lv_label_create(sb);
    lv_label_set_text(lbl_sb_bt, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_align(lbl_sb_bt, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_sb_bt, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sb_bt, COLOR_GRAY, 0);

    // Heure
    lbl_sb_time = lv_label_create(sb);
    lv_label_set_text(lbl_sb_time, "--:--");
    lv_obj_align(lbl_sb_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_sb_time, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sb_time, COLOR_BLUE, 0);

    // SOC batterie
    lbl_sb_soc = lv_label_create(sb);
    lv_label_set_text(lbl_sb_soc, "SOC --%");
    lv_obj_align(lbl_sb_soc, LV_ALIGN_RIGHT_MID, -58, 0);
    lv_obj_set_style_text_font(lbl_sb_soc, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sb_soc, COLOR_GREEN, 0);

    // Tension
    lbl_sb_volt = lv_label_create(sb);
    lv_label_set_text(lbl_sb_volt, "--V");
    lv_obj_align(lbl_sb_volt, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_sb_volt, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sb_volt, COLOR_GRAY, 0);
}
