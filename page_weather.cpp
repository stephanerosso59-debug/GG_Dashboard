/*
 * page_weather.cpp - Page Météo BME280 (Version corrigée)
 * 
 * Pattern : Fonctions globales (cohérent avec page_home, page_lights, etc.)
 * Remplace l'ancienne classe PageWeather qui n'était pas intégrée à UiManager
 * 
 * Dépendances :
 *   - weather.h / weather.cpp (API OpenWeatherMap + BME280)
 *   - bme280_alerts.h (Alertes : gel, condensation, humidité, pression)
 *   - bme280_history.h (Historique 24h pour graphique)
 *   - ui_helpers.h (ui_create_title_bar, ui_create_nav_bar)
 */

#include "page_weather.h"
#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"
#include "../../shared/weather/weather.h"
#include "../../shared/weather/bme280_sensor.h"
#include "bme280_alerts.h"
#include "bme280_history.h"

#include <cstring>

// ═══════════════════════════════════════════════════════════════════════
//  VARIABLES STATIQUES (Widgets LVGL)
// ═══════════════════════════════════════════════════════════════════════

// Zone alertes (dynamique)
lv_obj_t* weather_alert_container = nullptr;

// Carte température intérieure
lv_obj_t* weather_temp_arc = nullptr;
lv_obj_t* weather_temp_label = nullptr;
lv_obj_t* weather_comfort_label = nullptr;

// Min/Max 24h
lv_obj_t* weather_temp_min_label = nullptr;
lv_obj_t* weather_temp_max_label = nullptr;

// Environnement
lv_obj_t* weather_humidity_label = nullptr;
lv_obj_t* weather_humidity_trend = nullptr;
lv_obj_t* weather_pressure_label = nullptr;
lv_obj_t* weather_pressure_trend = nullptr;
lv_obj_t* weather_dew_point_bar = nullptr;
lv_obj_t* weather_dew_point_label = nullptr;

// Extérieur
lv_obj_t* weather_outdoor_icon = nullptr;
lv_obj_t* weather_outdoor_temp = nullptr;
lv_obj_t* weather_wind_label = nullptr;

// Graphique historique
lv_obj_t* weather_chart = nullptr;
lv_chart_series_t* weather_temp_series = nullptr;
lv_chart_series_t* weather_hum_series = nullptr;

// Instance alertes
BME280Alerts bmeAlerts;

// Instance historique
BME280History bmeHistory;

// ═══════════════════════════════════════════════════════════════════════
//  HELPERS INTERNES
// ═══════════════════════════════════════════════════════════════════════

// Couleur selon température
static lv_color_t getTempColor(float temp) {
    if (temp <= 2.0f)  return lv_color_hex(0x3498DB);  // Bleu - froid/gel
    if (temp <= 15.0f) return lv_color_hex(0x74B9FF;  // Bleu clair
    if (temp <= 25.0f) return lv_color_hex(0x2ECC71);  // Vert - confortable
    if (temp <= 35.0f) return lv_color_hex(0xF39C12);  // Orange - chaud
    return lv_color_hex(0xE74C3C);              // Rouge - très chaud
}

// Texte confort
static const char* getComfortText(float temp) {
    if (temp < 16.0f) return "Froid ❄️";
    if (temp < 20.0f) return "Frais ✅";
    if (temp < 26.0f) return "Confortable ✅";
    if (temp < 32.0f) return "Chaud ⚠️";
    return "Très chaud 🔥";
}

// Icône tendance
static const char* getTrendIcon(float trend) {
    if (trend > 0.5f)  return "↑";
    if (trend < -0.5f) return "↓";
    return "→";
}

// Créer un badge d'alerte
static lv_obj_t* createAlertBadge(lv_obj_t* parent, Alert& alert) {
    lv_obj_t* badge = lv_label_create(parent);
    
    // Format : "⚠ Message"
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s",
             alert.type == ALERT_FREEZE ? "❄️" :
             alert.type == ALERT_CONDENSATION ? "💧" :
             alert.type == ALERT_HIGH_HUMIDITY ? "💨" :
             alert.type == ALERT_LOW_PRESSURE ? "🌪️" : "🔥",
             alert.message);
    
    lv_label_set_text(badge, buf);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(badge, alert.color, 0);
    
    return badge;
}

// Créer une carte réutilisable
static lv_obj_t* createCard(lv_obj_t* parent, const char* title,
                       lv_coord_t x, lv_coord_t y,
                       lv_coord_t w = 140, lv_coord_t h = 120) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &ui.style_card, 0);
    
    // Titre carte
    if (title && title[0]) {
        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_opa(lbl, LV_OPA_70, 0);
        lv_obj_set_pos(lbl, 10, 6);
    }
    
    return card;
}

// ═══════════════════════════════════════════════════════════════════════
//  BUILD : Construction de la page
// ═══════════════════════════════════════════════════════════════════════

void page_weather_build(lv_obj_t* parent) {
    // Fond commun dashboard
    ui_draw_bg(parent);
    
    // Titre de page
    ui_create_title_bar(parent, LV_SYMBOL_BELL, "Météo BME280", COLOR_BLUE);
    
    // ── ZONE ALERTES (haut, hauteur dynamique) ────────────────────────
    weather_alert_container = lv_obj_create(parent);
    lv_obj_set_size(weather_alert_container, SCREEN_WIDTH - 40, 0);
    lv_obj_set_pos(weather_alert_container, 20, 52);
    lv_obj_set_style_bg_opa(weather_alert_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(weather_alert_container, 0, 0);
    lv_obj_set_style_pad_all(weather_alert_container, 0, 0);
    lv_obj_set_flex_flow(weather_alert_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weather_alert_container, 
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // ── CARTE TEMPÉRATURE INTÉRIEURE (gauche) ───────────────────────────
    lv_obj_t* cardTemp = createCard(parent, "INTÉRIEUR", 20, 90, 140, 130);
    
    // Arc gauge température
    weather_temp_arc = lv_arc_create(cardTemp);
    lv_obj_set_size(weather_temp_arc, 100, 100);
    lv_obj_set_pos(weather_temp_arc, 10, 22);
    lv_arc_set_range(weather_temp_arc, -10, 50);
    lv_arc_set_value(weather_temp_arc, 20);
    lv_arc_set_bg_angles(weather_temp_arc, 120, 60);  // 270° arc
    lv_obj_set_style_arc_width(weather_temp_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(weather_temp_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(weather_temp_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_clear_flag(weather_temp_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Label valeur centre arc
    weather_temp_label = lv_label_create(cardTemp);
    lv_label_set_text(weather_temp_label, "--.-°");
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_24, 0);
    lv_obj_center(weather_temp_label);
    lv_obj_align(weather_temp_label, LV_ALIGN_CENTER);
    lv_obj_set_pos(weather_temp_label, 10, 22);
    
    // Label confort
    weather_comfort_label = lv_label_create(cardTemp);
    lv_label_set_text(weather_comfort_label, "--");
    lv_obj_set_style_text_font(weather_comfort_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(weather_comfort_label, 10, 115);
    lv_obj_align(weather_comfort_label, LV_ALIGN_CENTER);
    lv_obj_set_width(weather_comfort_label, 100);
    lv_label_set_long_mode(weather_comfort_label, LV_LABEL_LONG_DOT);
    
    // Min/Max
    lv_obj_t* minmaxBox = lv_obj_create(cardTemp);
    lv_obj_set_size(minmaxBox, 105, 36);
    lv_obj_set_pos(minmaxBox, 118, 28);
    lv_obj_set_style_bg_opa(minmaxBox, LV_OPA_20, 0);
    lv_obj_set_style_radius(minmaxBox, 5, 0);
    lv_obj_set_style_border_width(minmaxBox, 0, 0);
    
    weather_temp_min_label = lv_label_create(minmaxBox);
    lv_label_set_text(weather_temp_min_label, "Min: --.-°");
    lv_obj_set_style_text_color(weather_temp_min_label, lv_color_hex(0x74b9ff), 0);
    lv_obj_set_style_text_font(weather_temp_min_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(weather_temp_min_label, 5, 5);
    
    weather_temp_max_label = lv_label_create(minmaxBox);
    lv_label_set_text(weather_temp_max_label, "Max: --.-°");
    lv_obj_set_style_text_color(weather_temp_max_label, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(weather_temp_max_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(weather_temp_max_label, 5, 20);
    
    // ── CARTE ENVIRONNEMENT (centre) ───────────────────────────────────
    lv_obj_t* cardEnv = createCard(parent, "ENVIRONNEMENT", 170, 90, 145, 130);
    
    // Humidité
    lv_obj_t* lblHum = lv_label_create(cardEnv);
    lv_label_set_text(lblHum, "Hum:");
    lv_obj_set_style_text_font(lblHum, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblHum, 10, 28);
    
    weather_humidity_label = lv_label_create(cardEnv);
    lv_label_set_text(weather_humidity_label, "--%");
    lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(weather_humidity_label, 45, 26);
    
    weather_humidity_trend = lv_label_create(cardEnv);
    lv_label_set_text(weather_humidity_trend, "-");
    lv_obj_set_style_text_font(weather_humidity_trend, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(weather_humidity_trend, 90, 30);
    
    // Pression
    lv_obj_t* lblPress = lv_label_create(cardEnv);
    lv_label_set_text(lblPress, "Baro:");
    lv_obj_set_style_text_font(lblPress, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblPress, 10, 52);
    
    weather_pressure_label = lv_label_create(cardEnv);
    lv_label_set_text(weather_pressure_label, "----");
    lv_obj_set_style_text_font(weather_pressure_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(weather_pressure_label, 48, 50);
    
    weather_pressure_trend = lv_label_create(cardEnv);
    lv_label_set_text(weather_pressure_trend, "-");
    lv_obj_set_style_text_font(weather_pressure_trend, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(weather_pressure_trend, 95, 54);
    
    // Point de rosée
    lv_obj_t* lblDew = lv_label_create(cardEnv);
    lv_label_set_text(lblDew, "Rosée:");
    lv_obj_set_style_text_font(lblDew, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lblDew, 10, 82);
    
    weather_dew_point_label = lv_label_create(cardEnv);
    lv_label_set_text(weather_dew_point_label, "--.-°");
    lv_obj_set_style_text_font(weather_dew_point_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(weather_dew_point_label, 50, 80);
    
    weather_dew_point_bar = lv_bar_create(cardEnv);
    lv_obj_set_size(weather_dew_point_bar, 125, 6);
    lv_obj_set_pos(weather_dew_point_bar, 10, 102);
    lv_bar_set_range(weather_dew_point_bar, 0, 20);
    lv_bar_set_value(weather_dew_point_bar, 10, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(weather_dew_point_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(weather_dew_point_bar, lv_color_hex(0x9b59b6), LV_PART_INDICATOR);
    
    // ── CARTE EXTÉRIEURE (droite) ──────────────────────────────────────
    lv_obj_t* cardOut = createCard(parent, "EXTÉRIEUR", 325, 90, 135, 130);
    
    // Icône météo extérieure
    weather_outdoor_icon = lv_label_create(cardOut);
    lv_label_set_text(weather_outdoor_icon, "☀️");
    lv_obj_set_style_text_font(weather_outdoor_icon, &lv_font_montserrat_36, 0);
    lv_obj_set_pos(weather_outdoor_icon, 20, 28);
    
    // Température extérieure
    lv_obj_t* lblExt = lv_label_create(cardOut);
    lv_label_set_text(lblExt, "Ext:");
    lv_obj_set_style_text_font(lblExt, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblExt, 70, 35);
    
    weather_outdoor_temp = lv_label_create(cardOut);
    lv_label_set_text(weather_outdoor_temp, "--°");
    lv_obj_set_style_text_font(weather_outdoor_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(weather_outdoor_temp, 98, 32);
    
    // Vent
    weather_wind_label = lv_label_create(cardOut);
    lv_label_set_text(weather_wind_label, "Vent: -- km/h");
    lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(weather_wind_label, 10, 85);
    
    // ── GRAPHIQUE HISTORIQUE 24H (bas) ─────────────────────────────────
    lv_obj_t* cardChart = lv_obj_create(parent);
    lv_obj_set_size(cardChart, SCREEN_WIDTH - 40, 110);
    lv_obj_set_pos(cardChart, 20, 230);
    lv_obj_add_style(cardChart, &ui.style_card, 0);
    
    // Titre chart
    lv_obj_t* chartTitle = lv_label_create(cardChart);
    lv_label_set_text(chartTitle, "Historique 24h");
    lv_obj_set_style_text_font(chartTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTitle, COLOR_GRAY, 0);
    lv_obj_set_pos(chartTitle, 10, 4);
    
    // Chart LVGL
    weather_chart = lv_chart_create(cardChart);
    lv_obj_set_size(weather_chart, SCREEN_WIDTH - 60, 80);
    lv_obj_set_pos(weather_chart, 10, 22);
    lv_chart_set_type(weather_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(weather_chart, LV_CHART_AXIS_PRIMARY_Y, 10, 30);   // Température
    lv_chart_set_range(weather_chart, LV_CHART_AXIS_SECONDARY_Y, 40, 90);  // Humidité
    lv_chart_set_point_count(weather_chart, 24);
    lv_obj_set_style_size(weather_chart, 0, LV_PART_INDICATOR);  // Pas de points
    
    // Séries
    weather_temp_series = lv_chart_add_series(weather_chart, lv_color_hex(0x2ecc71), LV_CHART_AXIS_PRIMARY_Y);
    weather_hum_series = lv_chart_add_series(weather_chart, lv_color_hex(0x3498db), LV_CHART_AXIS_SECONDARY_Y);
    
    // Initialiser vides
    for (int i = 0; i < 24; i++) {
        weather_temp_series->y_points[i] = LV_CHART_POINT_NONE;
        weather_hum_series->y_points[i] = LV_CHART_POINT_NONE;
    }
    
    // Barre de navigation
    ui_create_nav_bar(parent, PAGE_WEATHER);
    
    // Initialiser historique
    bmeHistory.begin();
}

// ═══════════════════════════════════════════════════════════════════════
//  UPDATE : Mise à jour des données (appelé dans loop())
// ═══════════════════════════════════════════════════════════════════════

void page_weather_update() {
    // Récupérer données BME280 depuis WeatherManager global
    const BME280Data& data = weatherManager.getData();
    
    if (!data.valid) {
        lv_label_set_text(weather_temp_label, "ERR");
        return;
    }
    
    // ── MISE À JOUR TEMPÉRATURE ────────────────────────────────────────
    float temp = data.temperature;
    lv_arc_set_value(weather_temp_arc, (int16_t)temp);
    lv_label_set_text_fmt(weather_temp_label, "%.1f°", temp);
    
    lv_color_t tempColor = getTempColor(temp);
    lv_obj_set_style_arc_color(weather_temp_arc, tempColor, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(weather_temp_label, tempColor, 0);
    lv_label_set_text(weather_comfort_label, getComfortText(temp));
    lv_obj_set_style_text_color(weather_comfort_label, tempColor, 0);
    
    // Min/Max 24h depuis historique
    lv_label_set_text_fmt(weather_temp_min_label, "Min: %.1f°", bmeHistory.getMinTemp24h());
    lv_label_set_text_fmt(weather_temp_max_label, "Max: %.1f°", bmeHistory.getMaxTemp24h());
    
    // ── MISE À JOUR ENVIRONNEMENT ──────────────────────────────────────
    lv_label_set_text_fmt(weather_humidity_label, "%.0f%%", data.humidity);
    
    float humTrend = bmeHistory.getHumidityTrend();
    lv_label_set_text(weather_humidity_trend, getTrendIcon(humTrend));
    lv_obj_set_style_text_color(weather_humidity_trend, 
        humTrend > 0.5 ? lv_color_hex(0xe74c3c) : lv_color_hex(0x3498db), 0);
    
    lv_label_set_text_fmt(weather_pressure_label, "%.0f", data.pressure);
    
    float pressTrend = bmeHistory.getPressureTrend();
    lv_label_set_text(weather_pressure_trend, getTrendIcon(pressTrend));
    
    // Point de rosée
    float dewPoint = BME280Alerts::calculateDewPoint(temp, data.humidity);
    float dewMargin = temp - dewPoint;
    
    lv_label_set_text_fmt(weather_dew_point_label, "%.1f°", dewPoint);
    lv_bar_set_value(weather_dew_point_bar, 
        (lv_coord_t)(dewMargin > 0 ? dewMargin : 0), LV_ANIM_OFF);
    
    // Couleur barre rosée selon marge
    lv_color_t dewColor = (dewMargin < 3.0f) ? 
        lv_color_hex(0xe74c3c) : lv_color_hex(0x9b59b6);
    lv_obj_set_style_bg_color(weather_dew_point_bar, dewColor, LV_PART_INDICATOR);
    
    // ── MISE À JOUR EXTÉRIEUR (API OWM) ────────────────────────────────
    if (weatherData.valid && weatherData.current.valid) {
        lv_label_set_text(weather_outdoor_icon, weatherData.current.icon_lv);
        lv_label_set_text_fmt(weather_outdoor_temp, "%.0f°", weatherData.current.temp);
        lv_label_set_text_fmt(weather_wind_label, "Vent: %.0f km/h %s",
            weatherData.current.wind_speed_kmh,
            wind_deg_to_dir(weatherData.current.wind_deg));
    }
    
    // ── MISE À JOUR GRAPHIQUE HISTORIQUE ───────────────────────────────
    const BME280History::HistoryPoint* hist = bmeHistory.getHistory24h();
    int count = bmeHistory.getHistoryCount();
    
    for (int i = 0; i < count && i < 24; i++) {
        weather_temp_series->y_points[i] = hist[i].temperature;
        weather_hum_series->y_points[i] = hist[i].humidity;
    }
    // Rafraîchir chart
    lv_chart_refresh(weather_chart);
    
    // ── VÉRIFICATION ALERTES ───────────────────────────────────────────
    bmeAlerts.check(temp, data.humidity, data.pressure, dewPoint);
    
    uint8_t alertCount = 0;
    Alert* alerts = bmeAlerts.getActiveAlerts(alertCount);
    
    // Nettoyer anciens badges
    lv_obj_clean(weather_alert_container);
    
    // Créer nouveaux badges actifs
    for (uint8_t i = 0; i < alertCount; i++) {
        createAlertBadge(weather_alert_container, alerts[i]);
    }
    
    // Ajouter données à l'historique (toutes les 5 min)
    bmeHistory.addPoint(temp, data.humidity, data.pressure);
}