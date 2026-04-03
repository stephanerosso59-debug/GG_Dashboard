
#include "page_weather.h"
#include <math.h>

PageWeather::PageWeather() 
    : page(nullptr), alertContainer(nullptr), tempArc(nullptr),
      tempLabel(nullptr), comfortLabel(nullptr), tempMinLabel(nullptr),
      tempMaxLabel(nullptr), humidityLabel(nullptr), humidityTrend(nullptr),
      pressureLabel(nullptr), pressureTrend(nullptr), dewPointBar(nullptr),
      dewPointLabel(nullptr), altitudeLabel(nullptr), chart(nullptr),
      tempSeries(nullptr), humSeries(nullptr) {
}

void PageWeather::create() {
    page = lv_obj_create(NULL);
    lv_obj_set_size(page, 480, 320);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x1a1a2e), 0);
    
    // Dégradé background
    lv_obj_set_style_bg_grad_color(page, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_grad_dir(page, LV_GRAD_DIR_VER, 0);
    
    // Header
    lv_obj_t* header = lv_label_create(page);
    lv_label_set_text(header, "METEO");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(header, 20, 10);
    
    // Zone alertes (haut)
    createAlertsArea(page);
    
    // Grille 3 cartes
    createTempCard(page);
    createEnvCard(page);
    createOutdoorCard(page);
    
    // Graphique historique (bas)
    createHistoryChart(page);
}

void PageWeather::createAlertsArea(lv_obj_t* parent) {
    alertContainer = lv_obj_create(parent);
    lv_obj_set_size(alertContainer, 440, 0); // Hauteur dynamique
    lv_obj_set_pos(alertContainer, 20, 40);
    lv_obj_set_style_bg_opa(alertContainer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(alertContainer, 0, 0);
    lv_obj_set_style_pad_all(alertContainer, 0, 0);
    lv_obj_set_flex_flow(alertContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(alertContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

void PageWeather::createTempCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, "INTERIEUR", 20, 90);
    
    // Arc température
    tempArc = lv_arc_create(card);
    lv_obj_set_size(tempArc, 100, 100);
    lv_obj_set_pos(tempArc, 10, 25);
    lv_arc_set_range(tempArc, -10, 50);
    lv_arc_set_value(tempArc, 20);
    lv_arc_set_bg_angles(tempArc, 120, 60); // 270° arc
    lv_obj_set_style_arc_width(tempArc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(tempArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(tempArc, lv_color_hex(0x333333), LV_PART_MAIN);
    
    // Label température centre
    tempLabel = lv_label_create(card);
    lv_label_set_text(tempLabel, "--.-°");
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(tempLabel);
    lv_obj_set_align(tempLabel, LV_ALIGN_CENTER);
    lv_obj_set_pos(tempLabel, 10, 25);
    
    // Texte confort
    comfortLabel = lv_label_create(card);
    lv_label_set_text(comfortLabel, "--");
    lv_obj_set_style_text_font(comfortLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(comfortLabel, 10, 115);
    lv_obj_set_align(comfortLabel, LV_ALIGN_CENTER);
    lv_obj_set_width(comfortLabel, 100);
    lv_label_set_long_mode(comfortLabel, LV_LABEL_LONG_DOT);
    
    // Min/Max
    lv_obj_t* minmax = lv_obj_create(card);
    lv_obj_set_size(minmax, 100, 40);
    lv_obj_set_pos(minmax, 120, 30);
    lv_obj_set_style_bg_opa(minmax, LV_OPA_20, 0);
    lv_obj_set_style_radius(minmax, 5, 0);
    lv_obj_set_style_border_width(minmax, 0, 0);
    
    tempMinLabel = lv_label_create(minmax);
    lv_label_set_text(tempMinLabel, "Min: --.-");
    lv_obj_set_style_text_color(tempMinLabel, lv_color_hex(0x74b9ff), 0);
    lv_obj_set_style_text_font(tempMinLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(tempMinLabel, 5, 5);
    
    tempMaxLabel = lv_label_create(minmax);
    lv_label_set_text(tempMaxLabel, "Max: --.-");
    lv_obj_set_style_text_color(tempMaxLabel, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_text_font(tempMaxLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(tempMaxLabel, 5, 22);
}

void PageWeather::createEnvCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, "ENV", 175, 90);
    
    // Humidité
    lv_obj_t* lblHum = lv_label_create(card);
    lv_label_set_text(lblHum, "Hum:");
    lv_obj_set_style_text_font(lblHum, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblHum, 10, 30);
    
    humidityLabel = lv_label_create(card);
    lv_label_set_text(humidityLabel, "--%");
    lv_obj_set_style_text_font(humidityLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(humidityLabel, 50, 28);
    
    humidityTrend = lv_label_create(card);
    lv_label_set_text(humidityTrend, "-");
    lv_obj_set_style_text_font(humidityTrend, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(humidityTrend, 95, 32);
    
    // Pression
    lv_obj_t* lblPress = lv_label_create(card);
    lv_label_set_text(lblPress, "Baro:");
    lv_obj_set_style_text_font(lblPress, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblPress, 10, 55);
    
    pressureLabel = lv_label_create(card);
    lv_label_set_text(pressureLabel, "----");
    lv_obj_set_style_text_font(pressureLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(pressureLabel, 50, 53);
    
    pressureTrend = lv_label_create(card);
    lv_label_set_text(pressureTrend, "-");
    lv_obj_set_style_text_font(pressureTrend, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(pressureTrend, 100, 57);
    
    // Point de rosée barre
    lv_obj_t* lblDew = lv_label_create(card);
    lv_label_set_text(lblDew, "Rosée:");
    lv_obj_set_style_text_font(lblDew, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(lblDew, 10, 85);
    
    dewPointLabel = lv_label_create(card);
    lv_label_set_text(dewPointLabel, "--.-");
    lv_obj_set_style_text_font(dewPointLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(dewPointLabel, 55, 84);
    
    dewPointBar = lv_bar_create(card);
    lv_obj_set_size(dewPointBar, 120, 6);
    lv_obj_set_pos(dewPointBar, 10, 105);
    lv_bar_set_range(dewPointBar, 0, 20);
    lv_bar_set_value(dewPointBar, 10, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(dewPointBar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dewPointBar, lv_color_hex(0x9b59b6), LV_PART_INDICATOR);
}

void PageWeather::createOutdoorCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, "EXTERIEUR", 330, 90);
    
    // Icône météo (texte pour l'instant)
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, "☀️");
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_pos(icon, 20, 30);
    
    // Température ext
    lv_obj_t* lblExt = lv_label_create(card);
    lv_label_set_text(lblExt, "Ext:");
    lv_obj_set_style_text_font(lblExt, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblExt, 70, 35);
    
    lv_obj_t* valExt = lv_label_create(card);
    lv_label_set_text(valExt, "--°");
    lv_obj_set_style_text_font(valExt, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(valExt, 100, 32);
    
    // Vent
    lv_obj_t* lblWind = lv_label_create(card);
    lv_label_set_text(lblWind, "Vent: -- km/h");
    lv_obj_set_style_text_font(lblWind, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lblWind, 10, 85);
}

void PageWeather::createHistoryChart(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 440, 110);
    lv_obj_set_pos(card, 20, 195);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    
    // Titre
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "24h");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(title, 10, 5);
    
    // Chart LVGL
    chart = lv_chart_create(card);
    lv_obj_set_size(chart, 420, 75);
    lv_obj_set_pos(chart, 10, 25);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 30);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 40, 90);
    lv_chart_set_point_count(chart, 24);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR); // Pas de points
    
    tempSeries = lv_chart_add_series(chart, lv_color_hex(0x2ecc71), LV_CHART_AXIS_PRIMARY_Y);
    humSeries = lv_chart_add_series(chart, lv_color_hex(0x3498db), LV_CHART_AXIS_SECONDARY_Y);
    
    // Initialiser avec données vides
    for (int i = 0; i < 24; i++) {
        tempSeries->y_points[i] = LV_CHART_POINT_NONE;
        humSeries->y_points[i] = LV_CHART_POINT_NONE;
    }
}

lv_obj_t* PageWeather::createCard(lv_obj_t* parent, const char* title, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 140, 95);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    
    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_70, 0);
    lv_obj_set_pos(lbl, 10, 8);
    
    return card;
}

void PageWeather::update(const WeatherManager& weather) {
    const BME280Data& data = weather.getBMEData();
    
    if (!data.valid) {
        lv_label_set_text(tempLabel, "ERR");
        return;
    }
    
    // Température
    updateTempDisplay(data.temperature);
    lv_label_set_text_fmt(tempMinLabel, "Min: %.1f°", weather.getMinTemp24h());
    lv_label_set_text_fmt(tempMaxLabel, "Max: %.1f°", weather.getMaxTemp24h());
    
    // Humidité avec tendance
    lv_label_set_text_fmt(humidityLabel, "%.0f%%", data.humidity);
    float humTrend = weather.getHistory().getTempTrend(); // Approximation
    lv_label_set_text(humidityTrend, getTrendIcon(humTrend));
    lv_obj_set_style_text_color(humidityTrend, 
        humTrend > 0 ? lv_color_hex(0xe74c3c) : lv_color_hex(0x3498db), 0);
    
    // Pression avec tendance
    lv_label_set_text_fmt(pressureLabel, "%.0f", data.pressure);
    float pressTrend = weather.getHistory().getPressureTrend();
    lv_label_set_text(pressureTrend, getTrendIcon(pressTrend));
    
    // Point de rosée
    updateDewPoint(data.temperature, data.humidity);
    
    // Alertes
    updateAlerts(weather);
    
    // Graphique
    updateChart(weather.getHistory());
}

void PageWeather::updateTempDisplay(float temp) {
    lv_arc_set_value(tempArc, (int16_t)temp);
    lv_label_set_text_fmt(tempLabel, "%.1f°", temp);
    
    lv_color_t color = getTempColor(temp);
    lv_obj_set_style_arc_color(tempArc, color, LV_PART_INDICATOR);
    
    lv_label_set_text(comfortLabel, getComfortText(temp));
    lv_obj_set_style_text_color(comfortLabel, color, 0);
}

void PageWeather::updateDewPoint(float temp, float humidity) {
    float dewPoint = BME280Alerts::calculateDewPoint(temp, humidity);
    float margin = temp - dewPoint;
    
    lv_label_set_text_fmt(dewPointLabel, "%.1f°", dewPoint);
    
    // Barre de risque: 20 = sécurité, 0 = condensation
    int barVal = (int)(margin * 5); // 4°C marge = 20 (max)
    if (barVal < 0) barVal = 0;
    if (barVal > 20) barVal = 20;
    lv_bar_set_value(dewPointBar, barVal, LV_ANIM_ON);
    
    // Couleur selon risque
    lv_color_t barColor;
    if (margin < 3) barColor = lv_color_hex(0xe74c3c);      // Rouge danger
    else if (margin < 5) barColor = lv_color_hex(0xf39c12); // Orange attention
    else barColor = lv_color_hex(0x9b59b6);                 // Violet OK
    
    lv_obj_set_style_bg_color(dewPointBar, barColor, LV_PART_INDICATOR);
}

void PageWeather::updateAlerts(const WeatherManager& weather) {
    // Vider conteneur
    lv_obj_clean(alertContainer);
    
    if (!weather.hasAnyAlert()) {
        lv_obj_set_height(alertContainer, 0);
        return;
    }
    
    uint8_t alertCount = 0;
    Alert* alerts = weather.getAlerts().getActiveAlerts(alertCount);
    
    lv_obj_set_height(alertContainer, alertCount * 25);
    
    for (int i = 0; i < 7; i++) {
        if (!alerts[i].active) continue;
        
        lv_obj_t* alert = lv_obj_create(alertContainer);
        lv_obj_set_size(alert, 440, 22);
        lv_obj_set_style_bg_color(alert, lv_color_hex(alerts[i].color), 0);
        lv_obj_set_style_radius(alert, 5, 0);
        lv_obj_set_style_border_width(alert, 0, 0);
        lv_obj_set_style_pad_all(alert, 2, 0);
        
        lv_obj_t* icon = lv_label_create(alert);
        lv_label_set_text(icon, alerts[i].icon);
        lv_obj_set_pos(icon, 5, 2);
        
        lv_obj_t* txt = lv_label_create(alert);
        lv_label_set_text(txt, alerts[i].title);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(txt, 30, 3);
    }
}

void PageWeather::updateChart(const BME280History& history) {
    uint8_t count = history.getCount();
    if (count < 2) return;
    
    // Afficher jusqu'à 24 points
    uint8_t displayCount = (count < 24) ? count : 24;
    
    for (uint8_t i = 0; i < displayCount; i++) {
        const HistoryPoint* pt = history.getSample(count - displayCount + i);
        if (pt) {
            tempSeries->y_points[i] = (lv_coord_t)pt->temperature;
            humSeries->y_points[i] = (lv_coord_t)pt->humidity;
        }
    }
    
    lv_chart_refresh(chart);
}

lv_color_t PageWeather::getTempColor(float temp) {
    if (temp < 15) return lv_color_hex(0x3498db);      // Bleu
    if (temp > 25) return lv_color_hex(0xe74c3c);      // Rouge
    if (temp >= 19 && temp <= 22) return lv_color_hex(0x2ecc71); // Vert
    return lv_color_hex(0xf39c12);                      // Orange
}

const char* PageWeather::getComfortText(float temp) {
    if (temp < 15) return "Froid";
    if (temp > 25) return "Chaud";
    if (temp >= 19 && temp <= 22) return "Confort";
    return "OK";
}

const char* PageWeather::getTrendIcon(float trend) {
    if (trend > 0.5) return "↑";
    if (trend < -0.5) return "↓";
    return "→";
}

void PageWeather::show() {
    if (page) lv_scr_load(page);
}

void PageWeather::hide() {
    // Géré par le gestionnaire de pages
}