/*
 * ui.cpp - Gestionnaire UI avec navigation 5 pages
 */
#include "ui.h"
#include "icons_sd.h"
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "display_config.h"
#include "jkbms_ble.h"
#include "heating_ble.h"
#include "victron_ble.h"
#include "modal_pages.h"

UiManager ui;

// Layout:
//   Status bar :    0..40  (40px)
//   Contenu :      40..520 (480px)
//   Nav bar :     520..600 (80px)
#define SB_HEIGHT   40
#define NAV_HEIGHT  80
#define CONTENT_Y   SB_HEIGHT
#define CONTENT_H   (SCREEN_HEIGHT - SB_HEIGHT - NAV_HEIGHT)

// ============================================================
//  Callback navigation
// ============================================================
static void _nav_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    DashPage p = (DashPage)(intptr_t)lv_obj_get_user_data(btn);
    ui.showPage(p);
}

// ============================================================
//  begin()
// ============================================================
void UiManager::begin() {
    _initStyles();
    _buildAllPages();
    _buildNavBar();
    showPage(PAGE_HOME);
    // Animations de démarrage : BLE qui pulse (pas encore connecté)
    if (lbl_sb_bt) ui_start_pulse(lbl_sb_bt);
}

// ============================================================
//  update()
// ============================================================
void UiManager::update() {
    // Mise à jour status bar
    bool wifi_ok = (WiFi.status() == WL_CONNECTED);

    // WiFi/Heure au milieu
    if (lbl_sb_time) {
        if (wifi_ok) {
            lv_label_set_text(lbl_sb_time, LV_SYMBOL_WIFI "  En ligne");
            lv_obj_set_style_text_color(lbl_sb_time, COLOR_GREEN, 0);
        } else {
            lv_label_set_text(lbl_sb_time, LV_SYMBOL_WIFI "  Recherche...");
            lv_obj_set_style_text_color(lbl_sb_time, COLOR_ORANGE, 0);
        }
    }

    // BLE status : vert si au moins un appareil BLE est vu
    if (lbl_sb_bt) {
        bool ble_active = jkbms_is_connected() || heating_is_connected() 
                          || victron_mppt_available() || victron_bmv_available();
        if (ble_active) {
            ui_stop_pulse(lbl_sb_bt);
            lv_obj_set_style_text_opa(lbl_sb_bt, LV_OPA_COVER, 0);
            lv_label_set_text(lbl_sb_bt, " BLE OK");
            lv_obj_set_style_text_color(lbl_sb_bt, COLOR_GREEN, 0);
        } else {
            ui_stop_pulse(lbl_sb_bt);
            lv_obj_set_style_text_opa(lbl_sb_bt, LV_OPA_COVER, 0);
            lv_label_set_text(lbl_sb_bt, " BLE --");
            lv_obj_set_style_text_color(lbl_sb_bt, COLOR_GRAY, 0);
        }
    }

    // SOC + tension : priorité Victron BMV (permanent) sinon JKBMS
    if (lbl_sb_soc && lbl_sb_volt) {
        float soc = -1, volt = -1;
        if (victron_bmv_available()) {
            soc  = victronBmvData.soc_pct;
            volt = victronBmvData.voltage;
        } else if (jkbmsData.valid) {
            soc  = jkbmsData.soc_pct;
            volt = jkbmsData.voltage;
        }
        
        if (soc >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "SOC %.0f%%", soc);
            lv_label_set_text(lbl_sb_soc, buf);
            lv_color_t c = COLOR_GREEN;
            if      (soc < 20) c = COLOR_RED;
            else if (soc < 40) c = COLOR_ORANGE;
            else if (soc < 60) c = COLOR_YELLOW;
            lv_obj_set_style_text_color(lbl_sb_soc, c, 0);

            snprintf(buf, sizeof(buf), "%.2fV", volt);
            lv_label_set_text(lbl_sb_volt, buf);
            lv_obj_set_style_text_color(lbl_sb_volt, COLOR_WHITE, 0);
        } else {
            lv_label_set_text(lbl_sb_soc, "SOC --%");
            lv_obj_set_style_text_color(lbl_sb_soc, COLOR_GRAY, 0);
            lv_label_set_text(lbl_sb_volt, "--V");
            lv_obj_set_style_text_color(lbl_sb_volt, COLOR_GRAY, 0);
        }
    }

    // Pages
    switch (_currentPage) {
        case PAGE_HOME:    page_home_update();    break;
        case PAGE_BATTERY: page_battery_update(); break;
        case PAGE_HEATING: page_heating_update(); break;
        case PAGE_HISTORY: page_history_update(); break;
        case PAGE_SYSTEM:  page_system_update();  break;
        default: break;
    }
    
    // Update modal si ouverte
    modal_update();
}

// ============================================================
//  showPage()
// ============================================================
void UiManager::showPage(DashPage page) {
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (_pages[i]) lv_obj_add_flag(_pages[i], LV_OBJ_FLAG_HIDDEN);
        if (_nav_btns[i]) lv_obj_remove_style(_nav_btns[i], &style_nav_btn_active, 0);
    }
    if (_pages[page]) lv_obj_clear_flag(_pages[page], LV_OBJ_FLAG_HIDDEN);
    if (_nav_btns[page]) lv_obj_add_style(_nav_btns[page], &style_nav_btn_active, 0);
    _currentPage = page;

    // ── Activation BLE JKBMS seulement sur page Energie ──
    if (page == PAGE_BATTERY) {
        jkbms_request_activate();
    } else {
        jkbms_request_deactivate();
    }
    
    // ── Activation BLE Chauffage seulement sur page Chauffage ──
    if (page == PAGE_HEATING) {
        heating_request_activate();
    } else {
        heating_request_deactivate();
    }
}

// ============================================================
//  _initStyles()
// ============================================================
void UiManager::_initStyles() {
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_BG_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_90);
    lv_style_set_border_color(&style_card, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 14);
    lv_style_set_pad_all(&style_card, 8);

    lv_style_init(&style_nav_btn);
    lv_style_set_bg_color(&style_nav_btn, lv_color_hex(0x21262D));
    lv_style_set_bg_opa(&style_nav_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_nav_btn, lv_color_hex(0x30363D));
    lv_style_set_border_width(&style_nav_btn, 1);
    lv_style_set_radius(&style_nav_btn, 8);

    lv_style_init(&style_nav_btn_active);
    lv_style_set_bg_color(&style_nav_btn_active, COLOR_BLUE);
    lv_style_set_bg_opa(&style_nav_btn_active, LV_OPA_COVER);
    lv_style_set_border_color(&style_nav_btn_active, COLOR_BLUE);
    lv_style_set_border_width(&style_nav_btn_active, 2);
    lv_style_set_radius(&style_nav_btn_active, 8);
}

// ============================================================
//  ui_draw_bg()
// ============================================================
void ui_draw_bg(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
}

// ============================================================
//  _buildAllPages()
// ============================================================
void UiManager::_buildAllPages() {
    lv_obj_t* screen = lv_scr_act();

    for (int i = 0; i < PAGE_COUNT; i++) {
        _pages[i] = lv_obj_create(screen);
        lv_obj_set_size(_pages[i], SCREEN_WIDTH, CONTENT_H);
        lv_obj_set_pos(_pages[i], 0, CONTENT_Y);
        lv_obj_set_style_bg_color(_pages[i], COLOR_BG_DARK, 0);
        lv_obj_set_style_bg_opa(_pages[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(_pages[i], 0, 0);
        lv_obj_set_style_radius(_pages[i], 0, 0);
        lv_obj_set_style_pad_all(_pages[i], 0, 0);
        lv_obj_clear_flag(_pages[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    page_home_build(_pages[PAGE_HOME]);
    page_lights_build(_pages[PAGE_LIGHTS]);
    page_battery_build(_pages[PAGE_BATTERY]);
    page_heating_build(_pages[PAGE_HEATING]);
    page_history_build(_pages[PAGE_HISTORY]);
    page_system_build(_pages[PAGE_SYSTEM]);

    // ── Status bar globale ─────────────────────────────────────
    lv_obj_t* sb = lv_obj_create(screen);
    lv_obj_set_size(sb, SCREEN_WIDTH, SB_HEIGHT);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x090D12), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_hor(sb, 16, 0);
    lv_obj_set_style_pad_ver(sb, 4, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    // BLE : icône (police icons) + texte (police fr)
    lv_obj_t* ble_icon = lv_label_create(sb);
    lv_label_set_text(ble_icon, "\xEF\x8A\x94");  // ICON_BLUETOOTH U+F294
    lv_obj_set_style_text_font(ble_icon, &lv_font_icons_22, 0);
    lv_obj_set_style_text_color(ble_icon, COLOR_BLUE, 0);
    lv_obj_align(ble_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_sb_bt = lv_label_create(sb);
    lv_label_set_text(lbl_sb_bt, " BLE --");
    lv_obj_align(lbl_sb_bt, LV_ALIGN_LEFT_MID, 28, 0);
    lv_obj_set_style_text_font(lbl_sb_bt, &lv_font_fr_18, 0);
    lv_obj_set_style_text_color(lbl_sb_bt, COLOR_GRAY, 0);

    lbl_sb_time = lv_label_create(sb);
    lv_label_set_text(lbl_sb_time, "--:--");
    lv_obj_align(lbl_sb_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_sb_time, &lv_font_fr_22, 0);
    lv_obj_set_style_text_color(lbl_sb_time, COLOR_WHITE, 0);

    lbl_sb_soc = lv_label_create(sb);
    lv_label_set_text(lbl_sb_soc, "SOC --%");
    lv_obj_align(lbl_sb_soc, LV_ALIGN_RIGHT_MID, -120, 0);
    lv_obj_set_style_text_font(lbl_sb_soc, &lv_font_fr_18, 0);
    lv_obj_set_style_text_color(lbl_sb_soc, COLOR_GREEN, 0);

    lbl_sb_volt = lv_label_create(sb);
    lv_label_set_text(lbl_sb_volt, "--V");
    lv_obj_align(lbl_sb_volt, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_set_style_text_font(lbl_sb_volt, &lv_font_fr_18, 0);
    lv_obj_set_style_text_color(lbl_sb_volt, COLOR_GRAY, 0);
    
    // Icône Historique cliquable (label simple)
    lv_obj_t* lbl_hist = lv_label_create(sb);
    lv_label_set_text(lbl_hist, LV_SYMBOL_LIST);
    lv_obj_align(lbl_hist, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_hist, &lv_font_fr_22, 0);
    lv_obj_set_style_text_color(lbl_hist, COLOR_BLUE, 0);
    lv_obj_add_flag(lbl_hist, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_hist, [](lv_event_t* e){
        ui.showPage(PAGE_HISTORY);
    }, LV_EVENT_CLICKED, nullptr);
}

// ============================================================
//  _buildNavBar()
// ============================================================
void UiManager::_buildNavBar() {
    lv_obj_t* screen = lv_scr_act();

    // Icônes : BMP depuis SD (preferred) avec fallback sur LV_SYMBOL standard
    static const struct {
        const char* icon;            // Symbole LVGL standard (fallback)
        const char* sd_icon_name;    // Nom de l'icone BMP sur SD
        const char* text;            // Texte court a afficher si rien d'autre
        DashPage page;
    } items[] = {
        { LV_SYMBOL_HOME,     "home",     "Accueil",   PAGE_HOME    },
        { LV_SYMBOL_EYE_OPEN, "lights",   "Lumières",  PAGE_LIGHTS  },
        { LV_SYMBOL_CHARGE,   "energy",   "Énergie",   PAGE_BATTERY },
        { LV_SYMBOL_POWER,    "heater",   "Chauffage", PAGE_HEATING },
        { LV_SYMBOL_SETTINGS, "settings", "Système",   PAGE_SYSTEM  },
    };
    const int N = 5;

    // Fond de la barre nav
    lv_obj_t* bar = lv_obj_create(screen);
    lv_obj_set_size(bar, SCREEN_WIDTH, NAV_HEIGHT);
    lv_obj_set_pos(bar, 0, SCREEN_HEIGHT - NAV_HEIGHT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    int btn_w = (SCREEN_WIDTH - 12 - (N-1)*6) / N;
    int btn_h = NAV_HEIGHT - 12;

    for (int i = 0; i < N; i++) {
        lv_obj_t* btn = lv_btn_create(bar);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, 6 + i * (btn_w + 6), 6);
        lv_obj_add_style(btn, &style_nav_btn, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)items[i].page);
        lv_obj_add_event_cb(btn, _nav_btn_cb, LV_EVENT_CLICKED, nullptr);
        _nav_btns[i] = btn;

        // Tenter d'utiliser l'icone BMP depuis SD
        const lv_img_dsc_t* sd_icon = icons_sd_get(items[i].sd_icon_name);
        if (sd_icon) {
            // Image BMP grande (64x64 -> ~52px) - icone seule, pas de texte
            lv_obj_t* img = lv_img_create(btn);
            lv_img_set_src(img, sd_icon);
            // Scale: 256 = 100% (64px), 208 = ~81% (52px)
            lv_img_set_zoom(img, 208);
            // Centree dans le bouton
            lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // FALLBACK 1 : si pas d'icone BMP, on affiche le symbole LVGL en haut
            //   et le texte du nom de page en bas (au moins on lit qcq chose)
            lv_obj_t* lbl_ic = lv_label_create(btn);
            lv_label_set_text(lbl_ic, items[i].icon);
            lv_obj_set_style_text_font(lbl_ic, &lv_font_fr_28, 0);
            lv_obj_set_style_text_color(lbl_ic, COLOR_WHITE, 0);
            lv_obj_align(lbl_ic, LV_ALIGN_TOP_MID, 0, 4);
            
            // FALLBACK 2 : nom du bouton en texte (toujours lisible)
            lv_obj_t* lbl_t = lv_label_create(btn);
            lv_label_set_text(lbl_t, items[i].text);
            lv_obj_set_style_text_font(lbl_t, &lv_font_fr_14, 0);
            lv_obj_set_style_text_color(lbl_t, COLOR_WHITE, 0);
            lv_obj_align(lbl_t, LV_ALIGN_BOTTOM_MID, 0, -4);
        }
        // Pas de label texte : icone seule plus jolie
    }
}

// ============================================================
//  Animations icones (pulse/blink)
// ============================================================

// Animation fade in/out
static void _anim_fade_cb(void* var, int32_t v) {
    lv_obj_set_style_text_opa((lv_obj_t*)var, v, 0);
}

void ui_start_pulse(lv_obj_t* obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 80, 255);
    lv_anim_set_time(&a, 700);
    lv_anim_set_playback_time(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, _anim_fade_cb);
    lv_anim_start(&a);
}

void ui_stop_pulse(lv_obj_t* obj) {
    lv_anim_del(obj, _anim_fade_cb);
    lv_obj_set_style_text_opa(obj, 255, 0);
}
