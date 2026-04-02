/*
 * page_heating.cpp - Page controle chauffage (Webasto / Espar via BLE)
 * Appareil BLE : c1:02:29:4f:fe:50
 */
#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"
#include "ble/heating_ble.h"

// ─── Timer auto-extinction chauffe-eau ──────────────────────────────────────
#define WATER_HEATER_TIMEOUT_MS  (2UL * 3600UL * 1000UL)  // 2h en ms

static bool     heater_on                  = false;
static float    target_temp                = 19.0f;
static uint32_t g_water_heater_start_ms    = 0;
static bool     g_water_heater_timer_active = false;

static lv_obj_t* sw_heater        = nullptr;
static lv_obj_t* lbl_heater_state = nullptr;
static lv_obj_t* lbl_target_temp  = nullptr;
static lv_obj_t* arc_target       = nullptr;
static lv_obj_t* lbl_set_temp     = nullptr;
static lv_obj_t* lbl_timer        = nullptr;  // décompte 2h
static lv_obj_t* bar_timer        = nullptr;  // barre progression timer

// ---- Icone flamme animee (partage avec van_ui_anim_init.cpp) ----
lv_obj_t* img_heating = nullptr;

static void _apply_heater() {
    // Commande BLE chauffage (c1:02:29:4f:fe:50)
    if (heater_on) {
        heatingBle.sendOn();
        heatingBle.sendTemp(target_temp);
        lv_label_set_text(lbl_heater_state, LV_SYMBOL_WARNING "  CHAUFFAGE ON");
        lv_obj_set_style_text_color(lbl_heater_state, COLOR_ORANGE, 0);
    } else {
        heatingBle.sendOff();
        lv_label_set_text(lbl_heater_state, "Chauffage OFF");
        lv_obj_set_style_text_color(lbl_heater_state, COLOR_GRAY, 0);
    }
}

static void cb_heater_switch(lv_event_t* e) {
    heater_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (heater_on) {
        // Démarrer le timer 2h
        g_water_heater_start_ms    = millis();
        g_water_heater_timer_active = true;
        if (lbl_timer) {
            lv_label_set_text(lbl_timer, LV_SYMBOL_STOP "  Arret dans 02:00:00");
            lv_obj_set_style_text_color(lbl_timer, COLOR_ORANGE, 0);
        }
        if (bar_timer) lv_bar_set_value(bar_timer, 100, LV_ANIM_OFF);
    } else {
        // Extinction manuelle → annuler timer
        g_water_heater_timer_active = false;
        if (lbl_timer) lv_label_set_text(lbl_timer, "");
        if (bar_timer) lv_bar_set_value(bar_timer, 0, LV_ANIM_OFF);
    }
    _apply_heater();
}

static void cb_temp_up(lv_event_t* e) {
    if (target_temp < HEATING_TEMP_MAX) {
        target_temp += HEATING_TEMP_STEP;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f °C", target_temp);
        lv_label_set_text(lbl_target_temp, buf);
        lv_arc_set_value(arc_target, (int)(target_temp));
        snprintf(buf, sizeof(buf), "Consigne: %.1f°C", target_temp);
        lv_label_set_text(lbl_set_temp, buf);
    }
}

static void cb_temp_down(lv_event_t* e) {
    if (target_temp > HEATING_TEMP_MIN) {
        target_temp -= HEATING_TEMP_STEP;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f °C", target_temp);
        lv_label_set_text(lbl_target_temp, buf);
        lv_arc_set_value(arc_target, (int)(target_temp));
        snprintf(buf, sizeof(buf), "Consigne: %.1f°C", target_temp);
        lv_label_set_text(lbl_set_temp, buf);
    }
}

static void cb_back_heating(lv_event_t* e) { ui.showPage(PAGE_HOME); }  // conservé pour compatibilité

// ============================================================
//  page_heating_build()
// ============================================================
void page_heating_build(lv_obj_t* parent) {
    ui_draw_bg(parent);

    // Titre (sans bouton retour — nav bar en bas remplace)
    lv_obj_t* lbl_title = lv_label_create(parent);
    lv_label_set_text(lbl_title, LV_SYMBOL_WARNING "  Chauffage & Eau");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_ORANGE, 0);
    lv_obj_set_pos(lbl_title, 12, 26);

    // ---- Carte principale ----
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 100);
    lv_obj_set_pos(card, 8, 52);
    lv_obj_add_style(card, &ui.style_card, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    // Switch ON/OFF
    lv_obj_t* lbl_sw = lv_label_create(card);
    lv_label_set_text(lbl_sw, "Alimentation :");
    lv_obj_set_style_text_font(lbl_sw, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sw, COLOR_WHITE, 0);
    lv_obj_set_pos(lbl_sw, 0, 0);

    sw_heater = lv_switch_create(card);
    lv_obj_set_pos(sw_heater, 200, -2);
    lv_obj_set_style_bg_color(sw_heater, COLOR_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw_heater, COLOR_ORANGE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_heater, cb_heater_switch, LV_EVENT_VALUE_CHANGED, nullptr);

    // Etat chauffage
    lbl_heater_state = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_heater_state, &lv_font_montserrat_18, 0);
    lv_label_set_text(lbl_heater_state, "Chauffage OFF");
    lv_obj_set_style_text_color(lbl_heater_state, COLOR_GRAY, 0);
    lv_obj_set_pos(lbl_heater_state, 0, 36);

    // ── Timer auto-extinction (décompte 2h) ──────────────────────────────────
    lbl_timer = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_timer, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_timer, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_timer, "");
    lv_obj_set_pos(lbl_timer, 0, 58);

    // Barre progression timer (pleine largeur de la carte)
    bar_timer = lv_bar_create(card);
    lv_obj_set_size(bar_timer, SCREEN_WIDTH - 60, 5);
    lv_obj_set_pos(bar_timer, 0, 74);
    lv_bar_set_range(bar_timer, 0, 100);
    lv_bar_set_value(bar_timer, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_timer, lv_color_hex(0x21262D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_timer, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_timer, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_timer, 2, LV_PART_INDICATOR);

    // Séparateur (décalé après barre timer)
    lv_obj_t* sep = lv_obj_create(card);
    lv_obj_set_size(sep, SCREEN_WIDTH - 50, 1);
    lv_obj_set_pos(sep, 0, 65);
    lv_obj_set_style_bg_color(sep, COLOR_GRAY, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_30, 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    // Arc consigne temperature
    arc_target = lv_arc_create(card);
    lv_arc_set_rotation(arc_target, 135);
    lv_arc_set_bg_angles(arc_target, 0, 270);
    lv_arc_set_range(arc_target, (int)HEATING_TEMP_MIN, (int)HEATING_TEMP_MAX);
    lv_arc_set_value(arc_target, (int)target_temp);
    lv_obj_set_size(arc_target, 150, 150);
    lv_obj_set_pos(arc_target, 40, 70);
    lv_obj_set_style_arc_color(arc_target, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_target, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_target, lv_color_hex(0x2A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_target, 10, LV_PART_MAIN);
    lv_obj_clear_flag(arc_target, LV_OBJ_FLAG_CLICKABLE);

    // Temperature cible (gros chiffre)
    lbl_target_temp = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_target_temp, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_target_temp, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_target_temp, "19.0 °C");
    lv_obj_set_pos(lbl_target_temp, 52, 130);

    // Label consigne
    lbl_set_temp = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_set_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_set_temp, COLOR_GRAY, 0);
    lv_label_set_text(lbl_set_temp, "Consigne: 19.0°C");
    lv_obj_set_pos(lbl_set_temp, 35, 178);

    // Boutons + / -
    lv_obj_t* btn_up = lv_btn_create(card);
    lv_obj_set_size(btn_up, 80, 60);
    lv_obj_set_pos(btn_up, 240, 90);
    lv_obj_set_style_bg_color(btn_up, COLOR_ORANGE, 0);
    lv_obj_set_style_bg_opa(btn_up, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_up, 12, 0);
    lv_obj_set_style_border_width(btn_up, 0, 0);
    lv_obj_add_event_cb(btn_up, cb_temp_up, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, "+");
    lv_obj_set_style_text_font(lbl_up, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_up, lv_color_white(), 0);
    lv_obj_center(lbl_up);

    lv_obj_t* btn_down = lv_btn_create(card);
    lv_obj_set_size(btn_down, 80, 60);
    lv_obj_set_pos(btn_down, 240, 162);
    lv_obj_set_style_bg_color(btn_down, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_bg_opa(btn_down, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_down, 12, 0);
    lv_obj_set_style_border_width(btn_down, 1, 0);
    lv_obj_set_style_border_color(btn_down, COLOR_ORANGE, 0);
    lv_obj_add_event_cb(btn_down, cb_temp_down, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, "-");
    lv_obj_set_style_text_font(lbl_down, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_down, COLOR_ORANGE, 0);
    lv_obj_center(lbl_down);

    // Plage info
    lv_obj_t* lbl_range = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_range, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_range, COLOR_GRAY, 0);
    char rbuf[32];
    snprintf(rbuf, sizeof(rbuf), "Min %.0f°C  |  Max %.0f°C", HEATING_TEMP_MIN, HEATING_TEMP_MAX);
    lv_label_set_text(lbl_range, rbuf);
    lv_obj_align(lbl_range, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Barre de navigation inférieure
    ui_create_nav_bar(parent, PAGE_HEATING);
}

// ============================================================
//  page_heating_update()
// ============================================================
void page_heating_update() {
    if (!g_water_heater_timer_active || !heater_on) return;

    uint32_t elapsed = millis() - g_water_heater_start_ms;
    if (elapsed >= WATER_HEATER_TIMEOUT_MS) {
        // Auto-extinction après 2h
        heater_on = false;
        g_water_heater_timer_active = false;
        heatingBle.sendOff();
        lv_obj_clear_state(sw_heater, LV_STATE_CHECKED);
        _apply_heater();
        if (lbl_timer) {
            lv_label_set_text(lbl_timer, LV_SYMBOL_WARNING "  Eteint auto (2h)");
            lv_obj_set_style_text_color(lbl_timer, COLOR_RED, 0);
        }
        if (bar_timer) lv_bar_set_value(bar_timer, 0, LV_ANIM_ON);
        return;
    }

    // Afficher le temps restant HH:MM:SS
    uint32_t remaining = (WATER_HEATER_TIMEOUT_MS - elapsed) / 1000;
    uint32_t h = remaining / 3600;
    uint32_t m = (remaining % 3600) / 60;
    uint32_t s = remaining % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), LV_SYMBOL_STOP "  Arret dans %02lu:%02lu:%02lu", h, m, s);
    if (lbl_timer) lv_label_set_text(lbl_timer, buf);

    // Barre de progression (100% = début, 0% = fin)
    uint32_t pct = (uint32_t)(100UL * (WATER_HEATER_TIMEOUT_MS - elapsed) / WATER_HEATER_TIMEOUT_MS);
    if (bar_timer) lv_bar_set_value(bar_timer, (int16_t)pct, LV_ANIM_OFF);
}
