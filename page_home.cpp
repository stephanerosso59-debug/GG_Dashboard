/*
 * page_home.cpp - Page d'accueil : van, heure, meteo enrichie, navigation
 * Meteo inspiree de : platinum-weather-card (Makin-Things)
 */
#include "ui.h"
#include "ui_helpers.h"
#include "../config.h"
#include "../../shared/weather/weather.h"
#include "../../shared/ble/jkbms_ble.h"
#include "../gps/gps.h"
#include <WiFi.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Widgets dynamiques
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t* lbl_clock       = nullptr;
static lv_obj_t* lbl_date        = nullptr;
static lv_obj_t* lbl_bt_status   = nullptr;
static lv_obj_t* lbl_wifi_status = nullptr;
static lv_obj_t* lbl_wifi_ip     = nullptr;

// ─── Carte GPS ───────────────────────────────────────────────────────────────
static lv_obj_t* lbl_gps_speed  = nullptr;
static lv_obj_t* lbl_gps_course = nullptr;
static lv_obj_t* lbl_gps_coord  = nullptr;
static lv_obj_t* lbl_gps_alt    = nullptr;
static lv_obj_t* lbl_gps_sat    = nullptr;
static lv_obj_t* led_bt          = nullptr;
static lv_obj_t* led_wifi        = nullptr;

// Icones animees (partagees avec van_ui_anim_init.cpp)
lv_obj_t* img_wifi       = nullptr;
lv_obj_t* img_ble        = nullptr;
lv_obj_t* img_weather[3] = {};

// ─── Carte meteo courante (section superieure) ──────────────────────────────
static lv_obj_t* lbl_cur_icon   = nullptr;  // Grande icone meteo courante
static lv_obj_t* lbl_cur_temp   = nullptr;  // Temperature actuelle
static lv_obj_t* lbl_cur_feels  = nullptr;  // Ressenti
static lv_obj_t* lbl_cur_desc   = nullptr;  // Description
static lv_obj_t* lbl_cur_wind   = nullptr;  // Vent vitesse + direction
static lv_obj_t* lbl_cur_humid  = nullptr;  // Humidite
static lv_obj_t* lbl_cur_pres   = nullptr;  // Pression
static lv_obj_t* lbl_sunrise    = nullptr;  // Lever soleil
static lv_obj_t* lbl_sunset     = nullptr;  // Coucher soleil

// ─── Previsions 3 jours ─────────────────────────────────────────────────────
static lv_obj_t* lbl_day[3]      = {};  // Nom du jour
static lv_obj_t* lbl_icon[3]     = {};  // Icone emoji
static lv_obj_t* lbl_tmax[3]     = {};  // Temp max
static lv_obj_t* lbl_tmin[3]     = {};  // Temp min
static lv_obj_t* lbl_humid[3]    = {};  // Humidite
static lv_obj_t* lbl_rain[3]     = {};  // Pluie mm
static lv_obj_t* lbl_wind_f[3]   = {};  // Vent (prevision)

// ─────────────────────────────────────────────────────────────────────────────
//  Callbacks navigation
// ─────────────────────────────────────────────────────────────────────────────
// Navigation via ui_create_nav_bar() dans ui_helpers.cpp

// ─────────────────────────────────────────────────────────────────────────────
//  page_home_build()
// ─────────────────────────────────────────────────────────────────────────────
void page_home_build(lv_obj_t* parent) {
    ui_draw_bg(parent);

    // ── Carte Horloge (haut gauche) ──────────────────────────────────────────
    lv_obj_t* card_clock = lv_obj_create(parent);
    lv_obj_set_size(card_clock, 160, 70);
    lv_obj_set_pos(card_clock, 8, 8);
    lv_obj_add_style(card_clock, &ui.style_card, 0);

    lbl_clock = lv_label_create(card_clock);
    lv_obj_add_style(lbl_clock, &ui.style_label_value, 0);
    lv_label_set_text(lbl_clock, "00:00");
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, 0, 0);

    lbl_date = lv_label_create(card_clock);
    lv_obj_add_style(lbl_date, &ui.style_label_unit, 0);
    lv_label_set_text(lbl_date, "Lun. 01 Jan");
    lv_obj_align(lbl_date, LV_ALIGN_BOTTOM_MID, 0, 0);

    // ── Carte Connectivite (haut milieu-gauche) ──────────────────────────────
    lv_obj_t* card_conn = lv_obj_create(parent);
    lv_obj_set_size(card_conn, 140, 70);
    lv_obj_set_pos(card_conn, 175, 8);
    lv_obj_add_style(card_conn, &ui.style_card, 0);

    led_bt = lv_led_create(card_conn);
    lv_led_set_color(led_bt, COLOR_BLUE);
    lv_obj_set_size(led_bt, 10, 10);
    lv_obj_align(led_bt, LV_ALIGN_TOP_LEFT, 0, 4);

    lbl_bt_status = lv_label_create(card_conn);
    lv_obj_set_style_text_font(lbl_bt_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_bt_status, COLOR_GRAY, 0);
    lv_label_set_text(lbl_bt_status, "BLE: ---");
    lv_obj_align(lbl_bt_status, LV_ALIGN_TOP_LEFT, 15, 2);

    led_wifi = lv_led_create(card_conn);
    lv_led_set_color(led_wifi, COLOR_GREEN);
    lv_obj_set_size(led_wifi, 10, 10);
    lv_obj_align(led_wifi, LV_ALIGN_TOP_LEFT, 0, 26);

    lbl_wifi_status = lv_label_create(card_conn);
    lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wifi_status, COLOR_GRAY, 0);
    lv_label_set_text(lbl_wifi_status, "WiFi: ---");
    lv_obj_align(lbl_wifi_status, LV_ALIGN_TOP_LEFT, 15, 24);

    lbl_wifi_ip = lv_label_create(card_conn);
    lv_obj_set_style_text_font(lbl_wifi_ip, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wifi_ip, COLOR_GRAY, 0);
    lv_label_set_text(lbl_wifi_ip, "");
    lv_obj_align(lbl_wifi_ip, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // ═══════════════════════════════════════════════════════════════════════
    //  CARTE METEO COURANTE (style platinum-weather-card)
    //  Position : haut droite   Taille : 280 x 70
    // ═══════════════════════════════════════════════════════════════════════
    lv_obj_t* card_cur = lv_obj_create(parent);
    lv_obj_set_size(card_cur, 280, 70);
    lv_obj_set_pos(card_cur, 322, 8);
    lv_obj_add_style(card_cur, &ui.style_card, 0);

    // Grande icone (30px, colonne gauche)
    lbl_cur_icon = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_cur_icon, COLOR_YELLOW, 0);
    lv_label_set_text(lbl_cur_icon, "\xE2\x98\x80");  // ☀ defaut
    lv_obj_set_pos(lbl_cur_icon, 2, 2);

    // Temperature grande
    lbl_cur_temp = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_temp, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_cur_temp, COLOR_WHITE, 0);
    lv_label_set_text(lbl_cur_temp, "--°");
    lv_obj_set_pos(lbl_cur_temp, 38, 2);

    // Ressenti (ligne 2 colonne 1)
    lbl_cur_feels = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_feels, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cur_feels, COLOR_GRAY, 0);
    lv_label_set_text(lbl_cur_feels, "res: --°");
    lv_obj_set_pos(lbl_cur_feels, 38, 38);

    // Description (ligne 2 colonne 2)
    lbl_cur_desc = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_desc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cur_desc, COLOR_ACCENT, 0);
    lv_label_set_text(lbl_cur_desc, "---");
    lv_obj_set_pos(lbl_cur_desc, 100, 38);

    // Vent (colonne droite haut)
    lbl_cur_wind = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_wind, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cur_wind, COLOR_WHITE, 0);
    lv_label_set_text(lbl_cur_wind, "\xF0\x9F\x92\xA8 -- km/h N");  // 💨
    lv_obj_set_pos(lbl_cur_wind, 190, 4);

    // Humidite (colonne droite milieu)
    lbl_cur_humid = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_humid, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cur_humid, lv_color_hex(0x3498DB), 0);
    lv_label_set_text(lbl_cur_humid, "\xF0\x9F\x92\xA7 --%");  // 💧
    lv_obj_set_pos(lbl_cur_humid, 190, 20);

    // Pression (colonne droite bas)
    lbl_cur_pres = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_cur_pres, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cur_pres, COLOR_GRAY, 0);
    lv_label_set_text(lbl_cur_pres, "--- hPa");
    lv_obj_set_pos(lbl_cur_pres, 190, 36);

    // Lever/coucher soleil (ligne bas complète)
    lbl_sunrise = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_sunrise, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sunrise, lv_color_hex(0xF7CC00), 0);
    lv_label_set_text(lbl_sunrise, "\xE2\x98\x80\xEF\xB8\x8F --:--");  // ☀️
    lv_obj_set_pos(lbl_sunrise, 2, 52);

    lbl_sunset = lv_label_create(card_cur);
    lv_obj_set_style_text_font(lbl_sunset, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sunset, lv_color_hex(0xE8651A), 0);
    lv_label_set_text(lbl_sunset, "\xF0\x9F\x8C\x99 --:--");  // 🌙
    lv_obj_set_pos(lbl_sunset, 80, 52);

    // ═══════════════════════════════════════════════════════════════════════
    //  PREVISIONS 3 JOURS (style platinum-weather-card — ligne horizontale)
    //  Position : y=85   Taille totale : 610 x 90
    // ═══════════════════════════════════════════════════════════════════════
    lv_obj_t* card_fcast = lv_obj_create(parent);
    lv_obj_set_size(card_fcast, 610, 90);
    lv_obj_set_pos(card_fcast, 8, 85);
    lv_obj_add_style(card_fcast, &ui.style_card, 0);

    // Separateurs verticaux
    for (int s = 1; s < 3; s++) {
        lv_obj_t* sep = lv_obj_create(card_fcast);
        lv_obj_set_size(sep, 1, 70);
        lv_obj_set_pos(sep, s * 203, 10);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x334455), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_radius(sep, 0, 0);
    }

    for (int i = 0; i < 3; i++) {
        int ox = i * 203 + 4;  // decalage horizontal par colonne

        // Nom du jour (haut centré)
        lbl_day[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_day[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_day[i], COLOR_ACCENT, 0);
        lv_label_set_text(lbl_day[i], i == 0 ? "Auj." : i == 1 ? "Dem." : "J+2");
        lv_obj_set_pos(lbl_day[i], ox + 75, 2);

        // Icone meteo (grand, gauche)
        lbl_icon[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_icon[i], &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl_icon[i], COLOR_YELLOW, 0);
        lv_label_set_text(lbl_icon[i], "?");
        lv_obj_set_pos(lbl_icon[i], ox + 2, 18);

        // Temp MAX (rouge/blanc)
        lbl_tmax[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_tmax[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_tmax[i], lv_color_hex(0xFF6B6B), 0);
        lv_label_set_text(lbl_tmax[i], "--°");
        lv_obj_set_pos(lbl_tmax[i], ox + 44, 18);

        // Temp MIN (bleu clair)
        lbl_tmin[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_tmin[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_tmin[i], lv_color_hex(0x74B9FF), 0);
        lv_label_set_text(lbl_tmin[i], "--°");
        lv_obj_set_pos(lbl_tmin[i], ox + 90, 22);

        // Humidite (bleu)
        lbl_humid[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_humid[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_humid[i], lv_color_hex(0x3498DB), 0);
        lv_label_set_text(lbl_humid[i], "\xF0\x9F\x92\xA7 --%");
        lv_obj_set_pos(lbl_humid[i], ox + 44, 48);

        // Pluie mm
        lbl_rain[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_rain[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_rain[i], lv_color_hex(0x74B9FF), 0);
        lv_label_set_text(lbl_rain[i], "0mm");
        lv_obj_set_pos(lbl_rain[i], ox + 44, 63);

        // Vent (prevision)
        lbl_wind_f[i] = lv_label_create(card_fcast);
        lv_obj_set_style_text_font(lbl_wind_f[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_wind_f[i], COLOR_GRAY, 0);
        lv_label_set_text(lbl_wind_f[i], "--km/h");
        lv_obj_set_pos(lbl_wind_f[i], ox + 120, 48);
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  CARTE GPS (NEO-6M) — Position : y=182   Taille : 620 x 68
    //  Affiche : vitesse | cap | coordonnées | altitude | satellites
    // ═══════════════════════════════════════════════════════════════════════
    lv_obj_t* card_gps = lv_obj_create(parent);
    lv_obj_set_size(card_gps, 620, 68);
    lv_obj_set_pos(card_gps, 8, 182);
    lv_obj_add_style(card_gps, &ui.style_card, 0);
    lv_obj_set_style_bg_color(card_gps, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_color(card_gps, lv_color_hex(0x1A3A5A), 0);
    lv_obj_clear_flag(card_gps, LV_OBJ_FLAG_SCROLLABLE);

    // Icone GPS
    lv_obj_t* lbl_gps_icon = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_gps_icon, lv_color_hex(0x58A6FF), 0);
    lv_label_set_text(lbl_gps_icon, LV_SYMBOL_GPS);
    lv_obj_set_pos(lbl_gps_icon, 2, 20);

    // Vitesse (grand)
    lbl_gps_speed = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_speed, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_gps_speed, lv_color_hex(0xE6EDF3), 0);
    lv_label_set_text(lbl_gps_speed, "-- km/h");
    lv_obj_set_pos(lbl_gps_speed, 28, 8);

    // Cap (direction)
    lbl_gps_course = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_course, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_gps_course, lv_color_hex(0xFFA600), 0);
    lv_label_set_text(lbl_gps_course, "Cap: --");
    lv_obj_set_pos(lbl_gps_course, 28, 44);

    // Coordonnees
    lbl_gps_coord = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_coord, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_gps_coord, lv_color_hex(0x8B949E), 0);
    lv_label_set_text(lbl_gps_coord, "--°N  --°E");
    lv_obj_set_pos(lbl_gps_coord, 200, 8);

    // Altitude
    lbl_gps_alt = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_alt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_gps_alt, lv_color_hex(0x8B949E), 0);
    lv_label_set_text(lbl_gps_alt, "Alt: -- m");
    lv_obj_set_pos(lbl_gps_alt, 200, 28);

    // Satellites
    lbl_gps_sat = lv_label_create(card_gps);
    lv_obj_set_style_text_font(lbl_gps_sat, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_gps_sat, lv_color_hex(0x3FB950), 0);
    lv_label_set_text(lbl_gps_sat, LV_SYMBOL_GPS " -- sat");
    lv_obj_set_pos(lbl_gps_sat, 200, 48);

    // ── Barre de navigation inférieure (persistante sur toutes les pages) ──────
    ui_create_nav_bar(parent, PAGE_HOME);
}

// ─────────────────────────────────────────────────────────────────────────────
//  page_home_update() - Appele dans loop()
// ─────────────────────────────────────────────────────────────────────────────
void page_home_update() {
    if (!lbl_clock) return;

    // ── Horloge ──────────────────────────────────────────────────────────────
    static uint32_t lastClockMs = 0;
    if (millis() - lastClockMs > 1000) {
        lastClockMs = millis();
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        if (t) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
            lv_label_set_text(lbl_clock, buf);
            static const char* jours[] = {"Dim","Lun","Mar","Mer","Jeu","Ven","Sam"};
            static const char* mois[]  = {"Jan","Fev","Mar","Avr","Mai","Jui",
                                           "Jul","Aou","Sep","Oct","Nov","Dec"};
            char dbuf[24];
            snprintf(dbuf, sizeof(dbuf), "%s %02d %s",
                     jours[t->tm_wday], t->tm_mday, mois[t->tm_mon]);
            lv_label_set_text(lbl_date, dbuf);
        }
    }

    // ── WiFi ─────────────────────────────────────────────────────────────────
    static uint32_t lastWifiMs = 0;
    if (millis() - lastWifiMs > 3000) {
        lastWifiMs = millis();
        if (WiFi.isConnected()) {
            lv_led_on(led_wifi);
            lv_label_set_text(lbl_wifi_status, "WiFi: OK");
            lv_obj_set_style_text_color(lbl_wifi_status, COLOR_GREEN, 0);
            lv_label_set_text(lbl_wifi_ip, WiFi.localIP().toString().c_str());
        } else {
            lv_led_off(led_wifi);
            lv_label_set_text(lbl_wifi_status, "WiFi: Off");
            lv_obj_set_style_text_color(lbl_wifi_status, COLOR_GRAY, 0);
            lv_label_set_text(lbl_wifi_ip, "");
        }
    }

    // ── BLE ──────────────────────────────────────────────────────────────────
    static uint32_t lastBleMs = 0;
    if (millis() - lastBleMs > 2000) {
        lastBleMs = millis();
        if (jkBms.isConnected()) {
            lv_led_on(led_bt);
            lv_label_set_text(lbl_bt_status, "BLE: JK+Vic");
            lv_obj_set_style_text_color(lbl_bt_status, COLOR_BLUE, 0);
        } else {
            lv_led_off(led_bt);
            lv_label_set_text(lbl_bt_status, "BLE: Off");
            lv_obj_set_style_text_color(lbl_bt_status, COLOR_GRAY, 0);
        }
    }

    // ── GPS ──────────────────────────────────────────────────────────────────
    if (lbl_gps_speed) {
        lv_label_set_text(lbl_gps_speed, gpsData.speed_str);
        char cap_buf[32];
        snprintf(cap_buf, sizeof(cap_buf), "Cap: %s (%.0f°)", gpsData.course_str, gpsData.course_deg);
        lv_label_set_text(lbl_gps_course, cap_buf);
        char coord_buf[28];
        snprintf(coord_buf, sizeof(coord_buf), "%s  %s", gpsData.lat_str, gpsData.lon_str);
        lv_label_set_text(lbl_gps_coord, coord_buf);
        char alt_buf[16];
        snprintf(alt_buf, sizeof(alt_buf), "Alt: %.0f m", gpsData.altitude_m);
        lv_label_set_text(lbl_gps_alt, alt_buf);
        char sat_buf[16];
        snprintf(sat_buf, sizeof(sat_buf), LV_SYMBOL_GPS " %d sat", gpsData.satellites);
        lv_label_set_text(lbl_gps_sat, sat_buf);
        lv_obj_set_style_text_color(lbl_gps_sat,
            gpsData.valid ? lv_color_hex(0x3FB950) : lv_color_hex(0xF85149), 0);
    }

    // ── Meteo ─────────────────────────────────────────────────────────────────
    static uint32_t lastWeatherMs = 0;
    if (millis() - lastWeatherMs > 60000) {
        lastWeatherMs = millis();
        if (!weatherData.valid) return;

        // --- Conditions courantes ---
        if (weatherData.current.valid) {
            WeatherCurrent& c = weatherData.current;

            // Icone coloree selon type meteo
            lv_obj_set_style_text_color(lbl_cur_icon, lv_color_hex(owm_id_to_color(c.owm_id)), 0);
            lv_label_set_text(lbl_cur_icon, c.icon_lv);

            char buf[48];
            snprintf(buf, sizeof(buf), "%.0f°", c.temp);
            lv_label_set_text(lbl_cur_temp, buf);

            snprintf(buf, sizeof(buf), "res: %.0f°", c.feels_like);
            lv_label_set_text(lbl_cur_feels, buf);

            lv_label_set_text(lbl_cur_desc, c.description);

            snprintf(buf, sizeof(buf), "\xF0\x9F\x92\xA8 %.0f km/h %s",
                     c.wind_speed_kmh, wind_deg_to_dir(c.wind_deg));
            lv_label_set_text(lbl_cur_wind, buf);

            snprintf(buf, sizeof(buf), "\xF0\x9F\x92\xA7 %d%%", c.humidity);
            lv_label_set_text(lbl_cur_humid, buf);

            snprintf(buf, sizeof(buf), "%.0f hPa", c.pressure_hpa);
            lv_label_set_text(lbl_cur_pres, buf);

            char sbuf[8];
            snprintf(buf, sizeof(buf), "\xE2\x98\x80\xEF\xB8\x8F %s",
                     format_sun_time(c.sunrise_ts, sbuf, sizeof(sbuf)));
            lv_label_set_text(lbl_sunrise, buf);

            snprintf(buf, sizeof(buf), "\xF0\x9F\x8C\x99 %s",
                     format_sun_time(c.sunset_ts, sbuf, sizeof(sbuf)));
            lv_label_set_text(lbl_sunset, buf);
        }

        // --- Previsions 3 jours ---
        for (int i = 0; i < 3; i++) {
            WeatherDay& wd = weatherData.days[i];
            char buf[32];

            // Nom du jour reel
            lv_label_set_text(lbl_day[i],
                              i == 0 ? "Auj." : wd.day_name[0] ? wd.day_name : "---");

            // Icone coloree
            lv_obj_set_style_text_color(lbl_icon[i], lv_color_hex(owm_id_to_color(wd.owm_id)), 0);
            lv_label_set_text(lbl_icon[i], wd.icon_lv);

            // Temp max (rouge)
            snprintf(buf, sizeof(buf), "%.0f°", wd.temp_max);
            lv_label_set_text(lbl_tmax[i], buf);

            // Temp min (bleu)
            snprintf(buf, sizeof(buf), "%.0f°", wd.temp_min);
            lv_label_set_text(lbl_tmin[i], buf);

            // Humidite
            snprintf(buf, sizeof(buf), "\xF0\x9F\x92\xA7 %d%%", wd.humidity);
            lv_label_set_text(lbl_humid[i], buf);

            // Pluie
            if (wd.rain_mm < 0.1f)
                lv_label_set_text(lbl_rain[i], "0 mm");
            else {
                snprintf(buf, sizeof(buf), "%.1f mm", wd.rain_mm);
                lv_label_set_text(lbl_rain[i], buf);
            }

            // Vent
            snprintf(buf, sizeof(buf), "%.0fkm/h %s",
                     wd.wind_speed_kmh, wind_deg_to_dir(wd.wind_deg));
            lv_label_set_text(lbl_wind_f[i], buf);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  page_status_update() - Met à jour la barre de statut globale (overlay)
//  Appelée à chaque itération de UiManager::update()
// ─────────────────────────────────────────────────────────────────────────────
void page_status_update() {
    if (!ui.lbl_sb_time) return;

    static uint32_t last_ms = 0;
    if (millis() - last_ms < 2000) return;
    last_ms = millis();

    // ── Heure ────────────────────────────────────────────────────────────────
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (t) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        lv_label_set_text(ui.lbl_sb_time, buf);
    }

    // ── BLE ──────────────────────────────────────────────────────────────────
    bool ble_ok = jkBms.isConnected();
    lv_label_set_text(ui.lbl_sb_bt, ble_ok ? LV_SYMBOL_BLUETOOTH " BLE" : LV_SYMBOL_BLUETOOTH " --");
    lv_obj_set_style_text_color(ui.lbl_sb_bt,
        ble_ok ? COLOR_BLUE : COLOR_GRAY, 0);

    // ── SOC batterie ─────────────────────────────────────────────────────────
    int soc = jkBms.getData().battery_soc;
    char buf_soc[12];
    snprintf(buf_soc, sizeof(buf_soc), "SOC %d%%", soc);
    lv_label_set_text(ui.lbl_sb_soc, buf_soc);
    lv_color_t soc_color = (soc > 60) ? COLOR_GREEN :
                           (soc > 30) ? COLOR_ORANGE : COLOR_RED;
    lv_obj_set_style_text_color(ui.lbl_sb_soc, soc_color, 0);

    // ── Tension ──────────────────────────────────────────────────────────────
    float volt = jkBms.getData().battery_voltage;
    char buf_v[10];
    snprintf(buf_v, sizeof(buf_v), "%.1fV", volt);
    lv_label_set_text(ui.lbl_sb_volt, buf_v);
}
