/* c - Dashboard 1024×600 - Grille 4 cols
 * 
 * LAYOUT (valeurs référence 480×320, multipliées par SX/SY):
 * 
 * ┌──────────────────────────────────────────────────────────┐
 * │ STATUS BAR (36px)                                        │
 * ├──────────────┬──────────────┬──────────────┬──────────────┤
 * │ CLOCK (R1C1)  │ CONNECT(R1C2)│ ALERTS(R1C3)│ VAN/GPS(R1C4)│ ← Row 1
 * ├──────────────┴──────────────┴──────────────┴──────────────┤
 * │ WEATHER (R2C1-2 large)                                │ ← Row 2
 * ├────────────────────────────┬──────────────┬──────────────┤
 * │ ENERGY (R3C1) │ LIGHTS (R3C2) │ HEATING (R3C3)│ ← Row 3
 * ├────────────────────────────┴──────────────┴──────────────┤
 * │ SYSTEM FOOTER (full width)                               │ ← Footer
 * └──────────────────────────────────────────────────────────┘
 */
#include "page_dashboard_7.h"
#include "ui_layout_helpers.h"
#include "../../shared/weather/weather.h"
#include "../../shared/ble/jkbms_ble.h"

lv_obj_t* dash7_lbl_clock=nullptr;
// ... autres widgets ...

void page_dashboard_7_build(lv_obj_t* parent){
    ui_draw_bg(parent);

    /* ═══ ROW 1: 4 colonnes égales ═══ */
    /* Col 1: Horloge grande */
    lv_obj_t* c_clk=ui_create_card(parent, 236, 140, 12, STATUS_BAR_HEIGHT+12);
    dash7_lbl_clock=ui_value_label(c_clk, "00:00");
    lv_obj_align(dash7_lbl_clock, LV_ALIGN_TOP_MID, 0, -15);

    /* Col 2: Connectivité */
    lv_obj_t* c_conn=ui_create_card(parent, 236, 140, 260, STATUS_BAR_HEIGHT+12);
    // BLE, WiFi, GPS labels...

    /* Col 3: Alertes */
    lv_obj_t* c_alr=ui_create_card(parent, 236, 140, 508, STATUS_BAR_HEIGHT+12);
    lv_obj_set_style_bg_color(c_alr, lv_color_hex(0x2D1810), 0);
    dash7_alert_box=lv_label_create(c_alr);

    /* Col 4: Van/Vitesse/GPS (NOUVEAU - 4ème colonne !) */
    lv_obj_t* c_van=ui_create_card(parent, 236, 140, 756, STATUS_BAR_HEIGHT+12);
    // Vitesse, cap, coordonnées, altitude...

    /* ═══ ROW 2: Météo (spans 3 cols) + Van (col 4) ═══ */
    lv_obj_t* c_wtr=ui_create_card(parent, 740, 152, 12, STATUS_BAR_HEIGHT+164);
    // Icone météo, température, ressenti, vent, humidité, pression, lever/coucher...

    lv_obj_t* c_van2=ui_create_card(parent, 236, 152, 764, STATUS_BAR_HEIGHT+164);
    // Silhouette van, vitesse, cap, GPS détaillé...

    /* ═══ ROW 3: Energy | Lights | Heating ═══ */
    lv_obj_t* c_eng=ui_create_card(parent, 236, 172, 12, STATUS_BAR_HEIGHT+332);
    dash7_arc_soc=lv_arc_create(c_eng);
    lv_arc_set_range(dash7_arc_soc, 0, 100);
    lv_obj_set_size(dash7_arc_soc, SX(120), SY(120));

    lv_obj_t* c_lgt=ui_create_card(parent, 236, 172, 260, STATUS_BAR_HEIGHT+332);
    // 7 lignes lumières compactes...

    lv_obj_t* c_heat=ui_create_card(parent, 236, 172, 508, STATUS_BAR_HEIGHT+332);
    dash7_lbl_heater=lv_label_create(c_heat);
    lv_obj_set_style_text_font(dash7_lbl_heater, &lv_font_montserrat_FONT_SIZE_VALUE, 0);
    lv_obj_align(dash7_lbl_heater, LV_ALIGN_CENTER, 0, 0);

    /* ═══ FOOTER: Système (full width) ═══ */
    lv_obj_t* c_sys=ui_create_card(parent, SCREEN_WIDTH-24, 48, 12, SCREEN_HEIGHT-STATUS_BAR_HEIGHT-60);
    // FW version, Uptime, Heap, WiFi RSSI, Chip temp...
}

void page_dashboard_7_update(){
    // Mise à jour horloge, SOC, température, alertes, lumières, chauffage...
}