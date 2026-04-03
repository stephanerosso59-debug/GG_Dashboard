#pragma once
/* page_dashboard_7.h - Dashboard unique pour Waveshare 7B 1024×600
 * Grille 4 colonnes × 3 rangées + status bar + footer système
 * 614,400 pixels disponibles → tout afficher sans navigation !
 */
#include "lvgl.h"
#include "ui.h"

void page_dashboard_7_build(lv_obj_t* parent);
void page_dashboard_7_update();

/* Widgets exposés pour mise à jour externe */
extern lv_obj_t* dash7_lbl_clock;
extern lv_obj_t* dash7_lbl_date;
extern lv_obj_t* dash7_lbl_temp;
extern lv_obj_t* dash7_lbl_soc;
extern lv_obj_t* dash7_arc_soc;
extern lv_obj_t* dash7_lbl_heater;
extern lv_obj_t* dash7_alert_box;