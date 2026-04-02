#pragma once
/*
 * ui_helpers.h - Widgets LVGL communs réutilisables
 */
#include <lvgl.h>
#include "ui.h"

// Bouton retour standard (glassmorphism) — conservé pour compatibilité
lv_obj_t* ui_create_back_btn(lv_obj_t* parent, lv_event_cb_t cb);

// Barre de titre : icone + texte (sans bouton retour — nav bar remplace)
lv_obj_t* ui_create_title_bar(lv_obj_t* parent, const char* icon, const char* text,
                               lv_color_t color);

// Barre de navigation inférieure persistante (56px, collée en bas)
// active_page : page mise en surbrillance (bleue)
void ui_create_nav_bar(lv_obj_t* parent, DashPage active_page);

// Ligne icone + valeur + unité (pour page_battery)
lv_obj_t* ui_make_val_row(lv_obj_t* parent, const char* icon, const char* unit,
                           int py, lv_color_t color);
