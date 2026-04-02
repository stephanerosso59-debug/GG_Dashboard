/*
 * page_system.cpp - Page Systeme ESP32 (Page 5)
 * Informations : uptime, heap, WiFi RSSI, BLE, firmware, temperature
 */
#include "ui_helpers.h"
#include "../config.h"
#include <WiFi.h>
#include <esp_system.h>

#define FW_VERSION "1.0.0"

static lv_obj_t* lbl_uptime   = nullptr;
static lv_obj_t* lbl_heap     = nullptr;
static lv_obj_t* lbl_rssi     = nullptr;
static lv_obj_t* lbl_ble      = nullptr;
static lv_obj_t* lbl_temp     = nullptr;

static void cb_back_system(lv_event_t* e) { ui.showPage(PAGE_HOME); }  // conservé pour compatibilité

// ═══════════════════════════════════════════════════════════════════════════════
void page_system_build(lv_obj_t* parent) {
    ui_draw_bg(parent);
    ui_create_title_bar(parent, LV_SYMBOL_SETTINGS, "Systeme ESP32", COLOR_GRAY);

    // Carte principale
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 24, SCREEN_HEIGHT - 60);
    lv_obj_set_pos(card, 12, 52);
    lv_obj_add_style(card, &ui.style_card, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    int py = 4;
    const int step = 32;

    // Firmware
    lv_obj_t* lbl_fw = lv_label_create(card);
    lv_label_set_text_fmt(lbl_fw, LV_SYMBOL_WARNING "  Firmware : v" FW_VERSION);
    lv_obj_set_style_text_font(lbl_fw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_fw, COLOR_ACCENT, 0);
    lv_obj_set_pos(lbl_fw, 0, py); py += step;

    // Uptime
    lbl_uptime = lv_label_create(card);
    lv_label_set_text(lbl_uptime, LV_SYMBOL_REFRESH "  Uptime : --");
    lv_obj_set_style_text_font(lbl_uptime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_uptime, COLOR_WHITE, 0);
    lv_obj_set_pos(lbl_uptime, 0, py); py += step;

    // Heap libre
    lbl_heap = lv_label_create(card);
    lv_label_set_text(lbl_heap, LV_SYMBOL_DRIVE "  Heap libre : --");
    lv_obj_set_style_text_font(lbl_heap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_heap, COLOR_GREEN, 0);
    lv_obj_set_pos(lbl_heap, 0, py); py += step;

    // WiFi RSSI
    lbl_rssi = lv_label_create(card);
    lv_label_set_text(lbl_rssi, LV_SYMBOL_WIFI "  WiFi : --");
    lv_obj_set_style_text_font(lbl_rssi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_rssi, COLOR_BLUE, 0);
    lv_obj_set_pos(lbl_rssi, 0, py); py += step;

    // BLE
    lbl_ble = lv_label_create(card);
    lv_label_set_text(lbl_ble, LV_SYMBOL_BLUETOOTH "  BLE : --");
    lv_obj_set_style_text_font(lbl_ble, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ble, COLOR_BLUE, 0);
    lv_obj_set_pos(lbl_ble, 0, py); py += step;

    // Temperature interne ESP32
    lbl_temp = lv_label_create(card);
    lv_label_set_text(lbl_temp, LV_SYMBOL_WARNING "  Temp. chip : --");
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_temp, COLOR_ORANGE, 0);
    lv_obj_set_pos(lbl_temp, 0, py);

    // Barre de navigation inférieure
    ui_create_nav_bar(parent, PAGE_SYSTEM);
}

// ═══════════════════════════════════════════════════════════════════════════════
void page_system_update() {
    if (!lbl_uptime) return;

    uint32_t s = millis() / 1000;
    uint32_t m = s / 60; s %= 60;
    uint32_t h = m / 60; m %= 60;
    lv_label_set_text_fmt(lbl_uptime, LV_SYMBOL_REFRESH "  Uptime : %02luh%02lum%02lus", h, m, s);

    uint32_t heap = esp_get_free_heap_size();
    lv_label_set_text_fmt(lbl_heap, LV_SYMBOL_DRIVE "  Heap libre : %lu Ko", heap / 1024);

    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        lv_label_set_text_fmt(lbl_rssi, LV_SYMBOL_WIFI "  WiFi : %s  %d dBm",
                              WiFi.SSID().c_str(), rssi);
        lv_obj_set_style_text_color(lbl_rssi, COLOR_GREEN, 0);
    } else {
        lv_label_set_text(lbl_rssi, LV_SYMBOL_WIFI "  WiFi : Deconnecte");
        lv_obj_set_style_text_color(lbl_rssi, COLOR_RED, 0);
    }

    // Température interne chip ESP32
    float temp_c = temperatureRead();
    lv_label_set_text_fmt(lbl_temp, LV_SYMBOL_WARNING "  Temp. chip : %.1f C", temp_c);
}