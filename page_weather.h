#pragma once
/*
 * page_weather.h - Page Météo BME280 (Pattern standard GG Dashboard)
 * 
 * Suit le même pattern que page_home, page_lights, etc.
 * Fonctions globales : page_weather_build() + page_weather_update()
 * 
 * Contenu :
 *   - Carte température intérieure (arc gauge)
 *   - Carte environnement (humidité, pression, point de rosée)
 *   - Carte extérieure (API OpenWeatherMap)
 *   - Zone alertes BME280 (gel, condensation, etc.)
 *   - Graphique historique 24h (température + humidité)
 */

#include "lvgl.h"
#include "ui.h"

// ============================================================
//  Build : Création des widgets LVGL de la page météo
// ============================================================
void page_weather_build(lv_obj_t* parent);

// ============================================================
//  Update : Mise à jour des valeurs (appelé dans loop via UiManager)
// ============================================================
void page_weather_update();

// ============================================================
//  Widgets exposés (pour accès externe si nécessaire)
// ============================================================

// Zone alertes
extern lv_obj_t* weather_alert_container;

// Carte température intérieure
extern lv_obj_t* weather_temp_arc;
extern lv_obj_t* weather_temp_label;
extern lv_obj_t* weather_comfort_label;

// Min/Max 24h
extern lv_obj_t* weather_temp_min_label;
extern lv_obj_t* weather_temp_max_label;

// Environnement
extern lv_obj_t* weather_humidity_label;
extern lv_obj_t* weather_humidity_trend;
extern lv_obj_t* weather_pressure_label;
extern lv_obj_t* weather_pressure_trend;
extern lv_obj_t* weather_dew_point_bar;
extern lv_obj_t* weather_dew_point_label;

// Extérieur
extern lv_obj_t* weather_outdoor_icon;
extern lv_obj_t* weather_outdoor_temp;
extern lv_obj_t* weather_wind_label;

// Graphique historique
extern lv_obj_t* weather_chart;
extern lv_chart_series_t* weather_temp_series;
extern lv_chart_series_t* weather_hum_series;