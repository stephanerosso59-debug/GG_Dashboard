/*
 * page_battery.cpp - Page Energie avec 4 graphiques
 * 1) Energy Distribution (donut 3 arcs)
 * 2) Energy Usage (barres PV/Conso)
 * 3) Solar Production (courbe ligne)
 * 4) Solar Consumed Gauge (arc %)
 * + BMV-712 / SmartShunt carte détaillée
 */
#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"
#include "../../shared/ble/jkbms_ble.h"
#include "../../shared/ble/victron_ble.h"

// ─── Historique énergie (buffer circulaire 12 x 5min = 1h) ───────────────────
#define HIST_SIZE 12
struct EnergyPoint { int16_t pv_w; int16_t cons_w; };
static EnergyPoint  energy_hist[HIST_SIZE] = {};
static uint8_t      hist_idx     = 0;
static uint32_t     hist_last_ms = 0;
#define HIST_INTERVAL_MS (5UL * 60UL * 1000UL)

// ─── Widgets : SOC Gauge ─────────────────────────────────────────────────────
static lv_obj_t* arc_soc     = nullptr;
static lv_obj_t* lbl_soc     = nullptr;
static lv_obj_t* lbl_soc_pct = nullptr;

// ─── Widgets : Energy Distribution (donut = 3 arcs superposés) ───────────────
static lv_obj_t* arc_dist_pv    = nullptr;  // vert — PV
static lv_obj_t* arc_dist_batt  = nullptr;  // bleu — Batterie
static lv_obj_t* arc_dist_cons  = nullptr;  // orange — Conso
static lv_obj_t* lbl_dist_total = nullptr;  // total W au centre
static lv_obj_t* lbl_dist_pv   = nullptr;
static lv_obj_t* lbl_dist_batt = nullptr;
static lv_obj_t* lbl_dist_cons = nullptr;

// ─── Widgets : Energy Usage (barres) ─────────────────────────────────────────
static lv_obj_t*         chart_usage  = nullptr;
static lv_chart_series_t* ser_pv      = nullptr;
static lv_chart_series_t* ser_cons    = nullptr;

// ─── Widgets : Solar Production (courbe ligne) ───────────────────────────────
static lv_obj_t*         chart_solar  = nullptr;
static lv_chart_series_t* ser_solar   = nullptr;
static lv_obj_t* lbl_solar_vpv   = nullptr;
static lv_obj_t* lbl_solar_ppv   = nullptr;
static lv_obj_t* lbl_solar_state = nullptr;

// ─── Widgets : Solar Consumed Gauge ──────────────────────────────────────────
static lv_obj_t* arc_solar_pct  = nullptr;
static lv_obj_t* lbl_solar_pct  = nullptr;
static lv_obj_t* lbl_solar_sub  = nullptr;

// ─── Widgets : BMV-712 / SmartShunt ──────────────────────────────────────────
static lv_obj_t* lbl_shunt_voltage  = nullptr;
static lv_obj_t* lbl_shunt_current  = nullptr;
static lv_obj_t* lbl_shunt_power    = nullptr;
static lv_obj_t* lbl_shunt_soc_val  = nullptr;
static lv_obj_t* lbl_shunt_ttg      = nullptr;
static lv_obj_t* lbl_consumed_ah    = nullptr;
static lv_obj_t* lbl_shunt_aux      = nullptr;
static lv_obj_t* lbl_shunt_trend    = nullptr;
static lv_obj_t* lbl_shunt_relay    = nullptr;
static lv_obj_t* lbl_shunt_alarm    = nullptr;
static lv_obj_t* bar_shunt_soc      = nullptr;

// ─── Icônes animées (partagées) ──────────────────────────────────────────────
lv_obj_t* img_battery = nullptr;
lv_obj_t* img_solar   = nullptr;

// ─── Alarme JKBMS ────────────────────────────────────────────────────────────
static lv_obj_t* lbl_alarm_box = nullptr;

// ─── Callback retour ─────────────────────────────────────────────────────────
static void cb_back_battery(lv_event_t* e) { ui.showPage(PAGE_HOME); }  // conservé pour compatibilité

// ═══════════════════════════════════════════════════════════════════════════════
//  page_battery_build()
// ═══════════════════════════════════════════════════════════════════════════════
void page_battery_build(lv_obj_t* parent) {
    ui_draw_bg(parent);

    // ── Titre (sans bouton retour — nav bar en bas) ───────────────────────────
    lv_obj_t* lbl_title = lv_label_create(parent);
    lv_label_set_text(lbl_title, LV_SYMBOL_BATTERY_FULL "  Energie");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_GREEN, 0);
    lv_obj_set_pos(lbl_title, 12, 26);

    int y_row1 = 52;
    int y_row2 = 196;
    int y_row3 = 328;

    // ═════════════════════════════════════════════════════════════════════════
    //  RANGÉE 1 : SOC Gauge + Distribution Donut + Energy Usage Barres
    // ═════════════════════════════════════════════════════════════════════════

    // ── 1A : SOC Gauge (gauche) ──────────────────────────────────────────────
    lv_obj_t* card_soc = lv_obj_create(parent);
    lv_obj_set_size(card_soc, 120, 148);
    lv_obj_set_pos(card_soc, 4, y_row1);
    lv_obj_add_style(card_soc, &ui.style_card, 0);

    arc_soc = lv_arc_create(card_soc);
    lv_arc_set_rotation(arc_soc, 135);
    lv_arc_set_bg_angles(arc_soc, 0, 270);
    lv_arc_set_range(arc_soc, 0, 100);
    lv_arc_set_value(arc_soc, 0);
    lv_obj_set_size(arc_soc, 90, 90);
    lv_obj_align(arc_soc, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_arc_color(arc_soc, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_soc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_soc, lv_color_hex(0x2A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_soc, 8, LV_PART_MAIN);
    lv_obj_clear_flag(arc_soc, LV_OBJ_FLAG_CLICKABLE);

    lbl_soc = lv_label_create(card_soc);
    lv_obj_set_style_text_font(lbl_soc, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_soc, COLOR_GREEN, 0);
    lv_label_set_text(lbl_soc, "--");
    lv_obj_align(lbl_soc, LV_ALIGN_TOP_MID, 0, 30);

    lbl_soc_pct = lv_label_create(card_soc);
    lv_obj_set_style_text_font(lbl_soc_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_soc_pct, COLOR_GRAY, 0);
    lv_label_set_text(lbl_soc_pct, "SOC %");
    lv_obj_align(lbl_soc_pct, LV_ALIGN_BOTTOM_MID, 0, -4);

    // ── 1B : Energy Distribution DONUT (3 arcs superposés) ───────────────────
    lv_obj_t* card_dist = lv_obj_create(parent);
    lv_obj_set_size(card_dist, 160, 148);
    lv_obj_set_pos(card_dist, 130, y_row1);
    lv_obj_add_style(card_dist, &ui.style_card, 0);

    // Label titre
    lv_obj_t* lb_dist_t = lv_label_create(card_dist);
    lv_label_set_text(lb_dist_t, "Distribution");
    lv_obj_set_style_text_font(lb_dist_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_dist_t, COLOR_ACCENT, 0);
    lv_obj_set_pos(lb_dist_t, 0, 0);

    // Arc 1 — PV (vert, extérieur)
    arc_dist_pv = lv_arc_create(card_dist);
    lv_arc_set_rotation(arc_dist_pv, 180);
    lv_arc_set_bg_angles(arc_dist_pv, 0, 360);
    lv_arc_set_range(arc_dist_pv, 0, 100);
    lv_arc_set_value(arc_dist_pv, 33);
    lv_obj_set_size(arc_dist_pv, 80, 80);
    lv_obj_align(arc_dist_pv, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_set_style_arc_color(arc_dist_pv, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_dist_pv, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_dist_pv, lv_color_hex(0x1A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_dist_pv, 10, LV_PART_MAIN);
    lv_obj_clear_flag(arc_dist_pv, LV_OBJ_FLAG_CLICKABLE);

    // Arc 2 — Batterie (bleu, milieu)
    arc_dist_batt = lv_arc_create(card_dist);
    lv_arc_set_rotation(arc_dist_batt, 180);
    lv_arc_set_bg_angles(arc_dist_batt, 0, 360);
    lv_arc_set_range(arc_dist_batt, 0, 100);
    lv_arc_set_value(arc_dist_batt, 33);
    lv_obj_set_size(arc_dist_batt, 56, 56);
    lv_obj_align(arc_dist_batt, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_arc_color(arc_dist_batt, COLOR_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_dist_batt, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_dist_batt, lv_color_hex(0x1A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_dist_batt, 8, LV_PART_MAIN);
    lv_obj_clear_flag(arc_dist_batt, LV_OBJ_FLAG_CLICKABLE);

    // Arc 3 — Conso (orange, intérieur)
    arc_dist_cons = lv_arc_create(card_dist);
    lv_arc_set_rotation(arc_dist_cons, 180);
    lv_arc_set_bg_angles(arc_dist_cons, 0, 360);
    lv_arc_set_range(arc_dist_cons, 0, 100);
    lv_arc_set_value(arc_dist_cons, 33);
    lv_obj_set_size(arc_dist_cons, 32, 32);
    lv_obj_align(arc_dist_cons, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_arc_color(arc_dist_cons, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_dist_cons, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_dist_cons, lv_color_hex(0x1A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_dist_cons, 6, LV_PART_MAIN);
    lv_obj_clear_flag(arc_dist_cons, LV_OBJ_FLAG_CLICKABLE);

    // Total W au centre
    lbl_dist_total = lv_label_create(card_dist);
    lv_obj_set_style_text_font(lbl_dist_total, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_dist_total, COLOR_WHITE, 0);
    lv_label_set_text(lbl_dist_total, "-- W");
    lv_obj_align(lbl_dist_total, LV_ALIGN_TOP_MID, 0, 44);

    // Légende (3 labels sous le donut)
    lbl_dist_pv = lv_label_create(card_dist);
    lv_obj_set_style_text_font(lbl_dist_pv, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dist_pv, COLOR_GREEN, 0);
    lv_label_set_text(lbl_dist_pv, "PV --W");
    lv_obj_set_pos(lbl_dist_pv, 0, 100);

    lbl_dist_batt = lv_label_create(card_dist);
    lv_obj_set_style_text_font(lbl_dist_batt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dist_batt, COLOR_BLUE, 0);
    lv_label_set_text(lbl_dist_batt, "Batt --W");
    lv_obj_set_pos(lbl_dist_batt, 0, 116);

    lbl_dist_cons = lv_label_create(card_dist);
    lv_obj_set_style_text_font(lbl_dist_cons, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dist_cons, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_dist_cons, "Conso --W");
    lv_obj_set_pos(lbl_dist_cons, 0, 132);

    // ── 1C : Energy Usage BARRES (chart bar) ─────────────────────────────────
    lv_obj_t* card_usage = lv_obj_create(parent);
    lv_obj_set_size(card_usage, 294, 148);
    lv_obj_set_pos(card_usage, 296, y_row1);
    lv_obj_add_style(card_usage, &ui.style_card, 0);

    lv_obj_t* lb_usage_t = lv_label_create(card_usage);
    lv_label_set_text(lb_usage_t, "Energy Usage (1h)");
    lv_obj_set_style_text_font(lb_usage_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_usage_t, COLOR_ACCENT, 0);
    lv_obj_set_pos(lb_usage_t, 0, 0);

    chart_usage = lv_chart_create(card_usage);
    lv_obj_set_size(chart_usage, 270, 110);
    lv_obj_set_pos(chart_usage, 4, 18);
    lv_chart_set_type(chart_usage, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart_usage, HIST_SIZE);
    lv_chart_set_range(chart_usage, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_chart_set_div_line_count(chart_usage, 4, 0);

    lv_obj_set_style_bg_color(chart_usage, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(chart_usage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_usage, 0, 0);
    lv_obj_set_style_line_color(chart_usage, lv_color_hex(0x1E2430), LV_PART_MAIN);
    lv_obj_set_style_pad_column(chart_usage, 2, 0);

    ser_pv   = lv_chart_add_series(chart_usage, COLOR_GREEN, LV_CHART_AXIS_PRIMARY_Y);
    ser_cons = lv_chart_add_series(chart_usage, COLOR_ORANGE, LV_CHART_AXIS_PRIMARY_Y);

    // Init avec 0
    for (int i = 0; i < HIST_SIZE; i++) {
        lv_chart_set_next_value(chart_usage, ser_pv, 0);
        lv_chart_set_next_value(chart_usage, ser_cons, 0);
    }

    // Légendes PV / Conso
    lv_obj_t* lb_leg_pv = lv_label_create(card_usage);
    lv_obj_set_style_text_font(lb_leg_pv, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_leg_pv, COLOR_GREEN, 0);
    lv_label_set_text(lb_leg_pv, "PV");
    lv_obj_set_pos(lb_leg_pv, 200, 0);

    lv_obj_t* lb_leg_co = lv_label_create(card_usage);
    lv_obj_set_style_text_font(lb_leg_co, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_leg_co, COLOR_ORANGE, 0);
    lv_label_set_text(lb_leg_co, "Conso");
    lv_obj_set_pos(lb_leg_co, 230, 0);

    // ═════════════════════════════════════════════════════════════════════════
    //  RANGÉE 2 : Solar Production (courbe) + Solar Consumed Gauge
    // ═════════════════════════════════════════════════════════════════════════

    // ── 2A : Solar Production LINE chart ─────────────────────────────────────
    lv_obj_t* card_solar = lv_obj_create(parent);
    lv_obj_set_size(card_solar, 316, 126);
    lv_obj_set_pos(card_solar, 4, y_row2);
    lv_obj_add_style(card_solar, &ui.style_card, 0);

    lv_obj_t* lb_solar_t = lv_label_create(card_solar);
    lv_label_set_text(lb_solar_t, "Solar Production");
    lv_obj_set_style_text_font(lb_solar_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_solar_t, COLOR_YELLOW, 0);
    lv_obj_set_pos(lb_solar_t, 0, 0);

    // Overlay labels (Vpv, Ppv, Etat MPPT)
    lbl_solar_vpv = lv_label_create(card_solar);
    lv_obj_set_style_text_font(lbl_solar_vpv, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_solar_vpv, COLOR_WHITE, 0);
    lv_label_set_text(lbl_solar_vpv, "-- V");
    lv_obj_set_pos(lbl_solar_vpv, 130, 0);

    lbl_solar_ppv = lv_label_create(card_solar);
    lv_obj_set_style_text_font(lbl_solar_ppv, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_solar_ppv, COLOR_YELLOW, 0);
    lv_label_set_text(lbl_solar_ppv, "-- W");
    lv_obj_set_pos(lbl_solar_ppv, 180, 0);

    lbl_solar_state = lv_label_create(card_solar);
    lv_obj_set_style_text_font(lbl_solar_state, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_solar_state, COLOR_GRAY, 0);
    lv_label_set_text(lbl_solar_state, "Off");
    lv_obj_set_pos(lbl_solar_state, 240, 0);

    chart_solar = lv_chart_create(card_solar);
    lv_obj_set_size(chart_solar, 296, 92);
    lv_obj_set_pos(chart_solar, 4, 18);
    lv_chart_set_type(chart_solar, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_solar, HIST_SIZE);
    lv_chart_set_range(chart_solar, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_chart_set_div_line_count(chart_solar, 3, 0);
    lv_chart_set_update_mode(chart_solar, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_bg_color(chart_solar, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(chart_solar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_solar, 0, 0);
    lv_obj_set_style_line_color(chart_solar, lv_color_hex(0x1E2430), LV_PART_MAIN);

    ser_solar = lv_chart_add_series(chart_solar, COLOR_YELLOW, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(chart_solar, 2, LV_PART_ITEMS);

    for (int i = 0; i < HIST_SIZE; i++)
        lv_chart_set_next_value(chart_solar, ser_solar, 0);

    // ── 2B : Solar Consumed Gauge (arc) ──────────────────────────────────────
    lv_obj_t* card_sg = lv_obj_create(parent);
    lv_obj_set_size(card_sg, 130, 126);
    lv_obj_set_pos(card_sg, 326, y_row2);
    lv_obj_add_style(card_sg, &ui.style_card, 0);

    lv_obj_t* lb_sg_t = lv_label_create(card_sg);
    lv_label_set_text(lb_sg_t, "PV Utilise");
    lv_obj_set_style_text_font(lb_sg_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_sg_t, COLOR_YELLOW, 0);
    lv_obj_set_pos(lb_sg_t, 0, 0);

    arc_solar_pct = lv_arc_create(card_sg);
    lv_arc_set_rotation(arc_solar_pct, 135);
    lv_arc_set_bg_angles(arc_solar_pct, 0, 270);
    lv_arc_set_range(arc_solar_pct, 0, 100);
    lv_arc_set_value(arc_solar_pct, 0);
    lv_obj_set_size(arc_solar_pct, 80, 80);
    lv_obj_align(arc_solar_pct, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_arc_color(arc_solar_pct, COLOR_YELLOW, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_solar_pct, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_solar_pct, lv_color_hex(0x2A2E35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_solar_pct, 8, LV_PART_MAIN);
    lv_obj_clear_flag(arc_solar_pct, LV_OBJ_FLAG_CLICKABLE);

    lbl_solar_pct = lv_label_create(card_sg);
    lv_obj_set_style_text_font(lbl_solar_pct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_solar_pct, COLOR_YELLOW, 0);
    lv_label_set_text(lbl_solar_pct, "--%");
    lv_obj_align(lbl_solar_pct, LV_ALIGN_TOP_MID, 0, 44);

    lbl_solar_sub = lv_label_create(card_sg);
    lv_obj_set_style_text_font(lbl_solar_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_solar_sub, COLOR_GRAY, 0);
    lv_label_set_text(lbl_solar_sub, "consomme");
    lv_obj_align(lbl_solar_sub, LV_ALIGN_BOTTOM_MID, 0, -4);

    // ═════════════════════════════════════════════════════════════════════════
    //  RANGÉE 3 : BMV-712 / SmartShunt (pleine largeur)
    // ═════════════════════════════════════════════════════════════════════════

    lv_obj_t* card_shunt = lv_obj_create(parent);
    lv_obj_set_size(card_shunt, SCREEN_WIDTH - 16, 88);
    lv_obj_set_pos(card_shunt, 4, y_row3);
    lv_obj_add_style(card_shunt, &ui.style_card, 0);

    lv_obj_t* lb_sh_t = lv_label_create(card_shunt);
    lv_label_set_text(lb_sh_t, "BMV-712 / SmartShunt");
    lv_obj_set_style_text_font(lb_sh_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lb_sh_t, COLOR_ACCENT, 0);
    lv_obj_set_pos(lb_sh_t, 0, 0);

    // Flèche tendance
    lbl_shunt_trend = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_trend, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_shunt_trend, COLOR_GRAY, 0);
    lv_label_set_text(lbl_shunt_trend, LV_SYMBOL_RIGHT);
    lv_obj_align(lbl_shunt_trend, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Grille horizontale : V | I | P | SOC | TTG | Conso | Aux | Relais | Alarme
    int cx = 0, cy = 18;
    #define SH_COL(w) cx; cx += (w)

    int x_v = SH_COL(72);
    lbl_shunt_voltage = ui_make_val_row(card_shunt, "V", "V", cy, COLOR_WHITE);
    lv_obj_set_pos(lv_obj_get_parent(lbl_shunt_voltage) == card_shunt ? lbl_shunt_voltage : lbl_shunt_voltage, 28, cy - 1);

    // Rearrange : utiliser des labels simples en grille
    // Ligne 1 : V, I, P, SOC bar
    lbl_shunt_voltage = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_voltage, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_shunt_voltage, COLOR_WHITE, 0);
    lv_label_set_text(lbl_shunt_voltage, "V: --");
    lv_obj_set_pos(lbl_shunt_voltage, 0, 18);

    lbl_shunt_current = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_current, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_shunt_current, COLOR_BLUE, 0);
    lv_label_set_text(lbl_shunt_current, "I: --");
    lv_obj_set_pos(lbl_shunt_current, 100, 18);

    lbl_shunt_power = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_power, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_shunt_power, COLOR_YELLOW, 0);
    lv_label_set_text(lbl_shunt_power, "P: --");
    lv_obj_set_pos(lbl_shunt_power, 200, 18);

    lbl_shunt_soc_val = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_soc_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_shunt_soc_val, COLOR_GREEN, 0);
    lv_label_set_text(lbl_shunt_soc_val, "SOC: --");
    lv_obj_set_pos(lbl_shunt_soc_val, 300, 18);

    // Barre SOC
    bar_shunt_soc = lv_bar_create(card_shunt);
    lv_bar_set_range(bar_shunt_soc, 0, 100);
    lv_bar_set_value(bar_shunt_soc, 0, LV_ANIM_OFF);
    lv_obj_set_size(bar_shunt_soc, 160, 5);
    lv_obj_set_pos(bar_shunt_soc, 390, 24);
    lv_obj_set_style_bg_color(bar_shunt_soc, lv_color_hex(0x2A2E35), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_shunt_soc, COLOR_GREEN, LV_PART_INDICATOR);

    // Ligne 2 : TTG, Conso, Aux, Relais, Alarme
    lbl_shunt_ttg = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_ttg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_shunt_ttg, COLOR_YELLOW, 0);
    lv_label_set_text(lbl_shunt_ttg, "TTG: --");
    lv_obj_set_pos(lbl_shunt_ttg, 0, 40);

    lbl_consumed_ah = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_consumed_ah, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_consumed_ah, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_consumed_ah, "Conso: --Ah");
    lv_obj_set_pos(lbl_consumed_ah, 100, 40);

    lbl_shunt_aux = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_aux, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_shunt_aux, COLOR_GRAY, 0);
    lv_label_set_text(lbl_shunt_aux, "Aux: --");
    lv_obj_set_pos(lbl_shunt_aux, 230, 40);

    lbl_shunt_relay = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_relay, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_shunt_relay, COLOR_GRAY, 0);
    lv_label_set_text(lbl_shunt_relay, "Relais: --");
    lv_obj_set_pos(lbl_shunt_relay, 350, 40);

    lbl_shunt_alarm = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_shunt_alarm, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_shunt_alarm, COLOR_GREEN, 0);
    lv_label_set_text(lbl_shunt_alarm, LV_SYMBOL_OK " OK");
    lv_obj_set_pos(lbl_shunt_alarm, 470, 40);

    // Ligne 3 : alarme JKBMS
    lbl_alarm_box = lv_label_create(card_shunt);
    lv_obj_set_style_text_font(lbl_alarm_box, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_alarm_box, COLOR_RED, 0);
    lv_label_set_text(lbl_alarm_box, "");
    lv_obj_set_pos(lbl_alarm_box, 0, 60);

    // Barre de navigation inférieure
    ui_create_nav_bar(parent, PAGE_BATTERY);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  page_battery_update()
// ═══════════════════════════════════════════════════════════════════════════════
void page_battery_update() {
    if (!arc_soc) return;

    static uint32_t lastMs = 0;
    if (millis() - lastMs < 1000) return;
    lastMs = millis();

    char buf[48];

    // ── JKBMS : SOC Gauge ────────────────────────────────────────────────────
    const JkBmsData& jk = jkBms.getData();
    if (jk.data_valid) {
        float soc = jk.battery_soc;
        lv_arc_set_value(arc_soc, (int)soc);
        snprintf(buf, sizeof(buf), "%.0f", soc);
        lv_label_set_text(lbl_soc, buf);
        lv_color_t sc = (soc < 20) ? COLOR_RED : (soc < 50 ? COLOR_YELLOW : COLOR_GREEN);
        lv_obj_set_style_arc_color(arc_soc, sc, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(lbl_soc, sc, 0);

        // Alarmes JKBMS
        String alarms = "";
        if (jk.overvoltage_protection)   alarms += "SURTENSION ";
        if (jk.undervoltage_protection)  alarms += "SOUS-TENSION ";
        if (jk.overcurrent_protection)   alarms += "SURCOURANT ";
        if (jk.overtemp_protection)      alarms += "SURCHAUFFE ";
        if (jk.short_circuit_protection) alarms += "COURT-CIRCUIT ";
        lv_label_set_text(lbl_alarm_box, alarms.c_str());
    }

    // ── Victron MPPT ─────────────────────────────────────────────────────────
    const VictronMpptData& mppt = victronBle.getMpptData();
    float pv_power = 0, charge_power = 0;
    if (mppt.data_valid) {
        pv_power     = mppt.pv_power;
        charge_power = mppt.charge_power;

        // Solar Production overlay
        snprintf(buf, sizeof(buf), "%.1fV", mppt.pv_voltage);
        lv_label_set_text(lbl_solar_vpv, buf);
        snprintf(buf, sizeof(buf), "%.0fW", pv_power);
        lv_label_set_text(lbl_solar_ppv, buf);
        static const char* states[] = {"Off","--","Defaut","Bulk","Absorp","Float"};
        lv_label_set_text(lbl_solar_state,
            (mppt.device_state <= 5) ? states[mppt.device_state] : "?");
    }

    // ── BMV-712 / SmartShunt ─────────────────────────────────────────────────
    const VictronShuntData& shunt = victronBle.getShuntData();
    float batt_power = 0;
    if (shunt.data_valid) {
        batt_power = shunt.battery_power;

        snprintf(buf, sizeof(buf), "V:%.2f", shunt.battery_voltage);
        lv_label_set_text(lbl_shunt_voltage, buf);

        snprintf(buf, sizeof(buf), "I:%.2fA", shunt.battery_current);
        lv_label_set_text(lbl_shunt_current, buf);
        lv_obj_set_style_text_color(lbl_shunt_current,
            shunt.battery_current > 0 ? COLOR_GREEN :
            shunt.battery_current < 0 ? COLOR_RED : COLOR_GRAY, 0);

        snprintf(buf, sizeof(buf), "P:%.0fW", batt_power);
        lv_label_set_text(lbl_shunt_power, buf);
        lv_obj_set_style_text_color(lbl_shunt_power,
            batt_power > 0 ? COLOR_GREEN : batt_power < 0 ? COLOR_RED : COLOR_GRAY, 0);

        snprintf(buf, sizeof(buf), "SOC:%.1f%%", shunt.soc);
        lv_label_set_text(lbl_shunt_soc_val, buf);
        lv_bar_set_value(bar_shunt_soc, (int)shunt.soc, LV_ANIM_ON);
        lv_color_t soc_col = shunt.soc < 20 ? COLOR_RED :
                             shunt.soc < 50 ? COLOR_YELLOW : COLOR_GREEN;
        lv_obj_set_style_bg_color(bar_shunt_soc, soc_col, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(lbl_shunt_soc_val, soc_col, 0);

        // TTG
        if (shunt.time_to_go < 0)
            lv_label_set_text(lbl_shunt_ttg, "TTG: --");
        else {
            int h = (int)(shunt.time_to_go / 60), m = (int)shunt.time_to_go % 60;
            snprintf(buf, sizeof(buf), "TTG:%dh%02d", h, m);
            lv_label_set_text(lbl_shunt_ttg, buf);
        }

        snprintf(buf, sizeof(buf), "Conso:%.1fAh", shunt.consumed_ah);
        lv_label_set_text(lbl_consumed_ah, buf);

        if (shunt.aux_is_temperature)
            snprintf(buf, sizeof(buf), "T:%.1fC", shunt.aux_temperature);
        else if (shunt.aux_voltage > 0.1f)
            snprintf(buf, sizeof(buf), "Mid:%.2fV", shunt.aux_voltage);
        else strncpy(buf, "Aux:--", sizeof(buf));
        lv_label_set_text(lbl_shunt_aux, buf);

        lv_label_set_text(lbl_shunt_relay,
            shunt.relay_state ? "Relais:ON" : "Relais:Off");
        lv_obj_set_style_text_color(lbl_shunt_relay,
            shunt.relay_state ? COLOR_YELLOW : COLOR_GRAY, 0);

        // Tendance
        if (shunt.trend > 0) {
            lv_label_set_text(lbl_shunt_trend, LV_SYMBOL_UP);
            lv_obj_set_style_text_color(lbl_shunt_trend, COLOR_GREEN, 0);
        } else if (shunt.trend < 0) {
            lv_label_set_text(lbl_shunt_trend, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_color(lbl_shunt_trend, COLOR_RED, 0);
        } else {
            lv_label_set_text(lbl_shunt_trend, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(lbl_shunt_trend, COLOR_GRAY, 0);
        }

        // Alarme
        if (!shunt.alarm) {
            lv_label_set_text(lbl_shunt_alarm, LV_SYMBOL_OK " OK");
            lv_obj_set_style_text_color(lbl_shunt_alarm, COLOR_GREEN, 0);
        } else {
            char ab[32] = "";
            if (shunt.alarm_reason & 0x01) strncat(ab, "SsV ", 4);
            if (shunt.alarm_reason & 0x02) strncat(ab, "SuV ", 4);
            if (shunt.alarm_reason & 0x04) strncat(ab, "SOC ", 4);
            lv_label_set_text(lbl_shunt_alarm, ab[0] ? ab : "!");
            lv_obj_set_style_text_color(lbl_shunt_alarm, COLOR_RED, 0);
        }
    }

    // ── Energy Distribution DONUT ────────────────────────────────────────────
    float abs_batt = (batt_power < 0) ? -batt_power : 0;
    float total = pv_power + abs_batt + 0.01f;  // eviter div/0
    int pct_pv   = (int)(pv_power  / total * 100);
    int pct_batt = (int)(abs_batt  / total * 100);
    int pct_cons = 100 - pct_pv - pct_batt;

    lv_arc_set_value(arc_dist_pv,   pct_pv);
    lv_arc_set_value(arc_dist_batt, pct_batt);
    lv_arc_set_value(arc_dist_cons, pct_cons > 0 ? pct_cons : 0);

    snprintf(buf, sizeof(buf), "%.0fW", pv_power + abs_batt);
    lv_label_set_text(lbl_dist_total, buf);
    snprintf(buf, sizeof(buf), "PV %.0fW", pv_power);
    lv_label_set_text(lbl_dist_pv, buf);
    snprintf(buf, sizeof(buf), "Batt %.0fW", abs_batt);
    lv_label_set_text(lbl_dist_batt, buf);
    snprintf(buf, sizeof(buf), "Conso %.0fW", pv_power + abs_batt);
    lv_label_set_text(lbl_dist_cons, buf);

    // ── Solar Consumed Gauge ─────────────────────────────────────────────────
    float cons_total = charge_power + ((batt_power < 0) ? -batt_power : 0) + 0.01f;
    int solar_pct = (int)(charge_power / cons_total * 100);
    if (solar_pct > 100) solar_pct = 100;
    if (solar_pct < 0) solar_pct = 0;
    lv_arc_set_value(arc_solar_pct, solar_pct);
    snprintf(buf, sizeof(buf), "%d%%", solar_pct);
    lv_label_set_text(lbl_solar_pct, buf);
    lv_color_t sg_col = solar_pct > 80 ? COLOR_YELLOW :
                        solar_pct > 50 ? COLOR_ORANGE : COLOR_GRAY;
    lv_obj_set_style_arc_color(arc_solar_pct, sg_col, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(lbl_solar_pct, sg_col, 0);

    // ── Historique : mise à jour toutes les 5 min ────────────────────────────
    if (millis() - hist_last_ms > HIST_INTERVAL_MS) {
        hist_last_ms = millis();
        energy_hist[hist_idx % HIST_SIZE] = {
            (int16_t)pv_power,
            (int16_t)((batt_power < 0) ? -batt_power : batt_power)
        };
        hist_idx++;

        // Rafraîchir les charts barres + ligne
        for (int i = 0; i < HIST_SIZE; i++) {
            int idx = (hist_idx + i) % HIST_SIZE;
            ser_pv->y_points[i]    = energy_hist[idx].pv_w;
            ser_cons->y_points[i]  = energy_hist[idx].cons_w;
            ser_solar->y_points[i] = energy_hist[idx].pv_w;
        }
        lv_chart_refresh(chart_usage);
        lv_chart_refresh(chart_solar);

        // Auto-range : trouver max
        int16_t ymax = 100;
        for (int i = 0; i < HIST_SIZE; i++) {
            if (energy_hist[i].pv_w > ymax)   ymax = energy_hist[i].pv_w;
            if (energy_hist[i].cons_w > ymax)  ymax = energy_hist[i].cons_w;
        }
        ymax = ((ymax / 50) + 1) * 50;  // arrondir au 50 supérieur
        lv_chart_set_range(chart_usage, LV_CHART_AXIS_PRIMARY_Y, 0, ymax);
        lv_chart_set_range(chart_solar, LV_CHART_AXIS_PRIMARY_Y, 0, ymax);
    }
}
