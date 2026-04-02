/*
 * page_lights.cpp - Page controle eclairages (6 lumieres + TV)
 * v1.1 - Correction: Ajout RELAY_LIGHT6 manquant
 */

#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"

// ✅ Tableau complet: 6 lumieres + 1 TV = 7 relais
static const int RELAY_PINS[] = {
    RELAY_LIGHT1,   // Salon
    RELAY_LIGHT2,   // Cuisine
    RELAY_LIGHT3,   // Chambre
    RELAY_LIGHT4,   // WC
    RELAY_LIGHT5,   // Ext. Avant
    RELAY_LIGHT6,   // Ext. Arriere ✨ NOUVEAU
    RELAY_TV         // Television
};

static const char* RELAY_NAMES[] = {
    "Salon",
    "Cuisine",
    "Chambre",
    "WC",
    "Ext. Avant",
    "Ext. Arriere",  // ✨ NOUVEAU
    "Television"
};

// ✅ CORRIGE: 7 relais (etait 6 avant)
#define NUM_RELAYS 7

static bool relay_state[NUM_RELAYS] = {false};

static lv_obj_t* sw_relay[NUM_RELAYS]  = {};
static lv_obj_t* led_relay[NUM_RELAYS] = {};
static lv_obj_t* lbl_relay[NUM_RELAYS] = {};

// ---- Callback switch ----
static void cb_switch(lv_event_t* e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t* sw = lv_event_get_target(e);
    
    relay_state[idx] = lv_obj_has_state(sw, LV_STATE_CHECKED);
    digitalWrite(RELAY_PINS[idx], relay_state[idx] ? RELAY_ON : RELAY_OFF);

    if (relay_state[idx]) {
        lv_led_on(led_relay[idx]);
        lv_obj_set_style_text_color(lbl_relay[idx], COLOR_YELLOW, 0);
    } else {
        lv_led_off(led_relay[idx]);
        lv_obj_set_style_text_color(lbl_relay[idx], COLOR_GRAY, 0);
    }
}

// ---- Callback "Tout eteindre" ----
static void cb_all_off(lv_event_t* e) {
    for (int i = 0; i < NUM_RELAYS; i++) {
        relay_state[i] = false;
        digitalWrite(RELAY_PINS[i], RELAY_OFF);
        lv_obj_clear_state(sw_relay[i], LV_STATE_CHECKED);
        lv_led_off(led_relay[i]);
        lv_obj_set_style_text_color(lbl_relay[i], COLOR_GRAY, 0);
    }
}

// ============================================================
//  page_lights_build()
// ============================================================
void page_lights_build(lv_obj_t* parent) {
    ui_draw_bg(parent);

    ui_create_title_bar(parent, LV_SYMBOL_SETTINGS, "Eclairage & TV", COLOR_YELLOW);

    int col_w = (SCREEN_WIDTH - 40) / 2;
    int row_h = 54;
    int x_start = 12;
    int y_start = 58;

    for (int i = 0; i < NUM_RELAYS; i++) {
        int col = i % 2;
        int row = i / 2;
        int px = x_start + col * (col_w + 8);
        int py = y_start + row * (row_h + 6);

        // TV centrée sur toute la largeur
        if (i == NUM_RELAYS - 1) {
            px = x_start + col_w / 2 - 10;
        }

        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, (i == NUM_RELAYS - 1) ? col_w + 8 : col_w, row_h);
        lv_obj_set_pos(card, (i == NUM_RELAYS - 1) ? (SCREEN_WIDTH - col_w - 8) / 2 : px, py);
        lv_obj_add_style(card, &ui.style_card, 0);
        lv_obj_set_pad_all(card, 6, 0);

        // LED indicateur
        led_relay[i] = lv_led_create(card);
        lv_led_set_color(led_relay[i], (i == NUM_RELAYS - 1) ? COLOR_BLUE : COLOR_YELLOW);
        lv_obj_set_size(led_relay[i], 10, 10);
        lv_obj_align(led_relay[i], LV_ALIGN_LEFT_MID, 0, 0);
        lv_led_off(led_relay[i]);

        // Nom
        lbl_relay[i] = lv_label_create(card);
        lv_label_set_text(lbl_relay[i], RELAY_NAMES[i]);
        lv_obj_set_style_text_font(lbl_relay[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_relay[i], COLOR_GRAY, 0);
        lv_obj_align(lbl_relay[i], LV_ALIGN_LEFT_MID, 18, 0);

        // Switch
        sw_relay[i] = lv_switch_create(card);
        lv_obj_align(sw_relay[i], LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(sw_relay[i], COLOR_GRAY, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sw_relay[i], COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw_relay[i], cb_switch, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)i);
    }

    // Bouton "Tout eteindre"
    lv_obj_t* btn_off = lv_btn_create(parent);
    lv_obj_set_size(btn_off, 160, 40);
    lv_obj_align(btn_off, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(btn_off, COLOR_RED, 0);
    lv_obj_set_style_bg_opa(btn_off, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_off, 10, 0);
    lv_obj_set_style_border_width(btn_off, 0, 0);
    lv_obj_add_event_cb(btn_off, cb_all_off, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_off = lv_label_create(btn_off);
    lv_label_set_text(lbl_off, LV_SYMBOL_POWER "  Tout eteindre");
    lv_obj_set_style_text_font(lbl_off, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_off, COLOR_WHITE, 0);
    lv_obj_center(lbl_off);

    // Init GPIO
    for (int i = 0; i < NUM_RELAYS; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], RELAY_OFF);
    }

    ui_create_nav_bar(parent, PAGE_LIGHTS);
}