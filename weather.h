#pragma once
/*
 * weather.h - Meteo enrichie via OpenWeatherMap API
 * Inspire de : platinum-weather-card (Makin-Things)
 *
 * Donnees par jour : temp min/max, ressenti, humidite, pression,
 * vent (vitesse+direction), pluie, UV, lever/coucher soleil, code icone.
 */
#include <Arduino.h>

// ─── Donnees par jour ─────────────────────────────────────────────────────────
struct WeatherDay {
    // Temperatures
    float    temp_max;
    float    temp_min;
    float    feels_like;        // Ressenti (jour courant uniquement, 0 sinon)

    // Humidite / Pression
    int      humidity;          // %
    float    pressure_hpa;      // hPa

    // Vent
    float    wind_speed_kmh;    // km/h
    int      wind_deg;          // 0-360
    float    wind_gust_kmh;     // km/h (rafales)

    // Precipitation
    float    rain_mm;           // mm sur la periode

    // Indice UV (entier 0-11+)
    int      uv_index;

    // Description / icone
    char     description[48];
    char     icon_lv[8];        // Emoji UTF-8 LVGL
    int      owm_id;            // Code OWM (ex: 800 = ciel clair)

    // Nom du jour (ex: "Lun", "Mar"...)
    char     day_name[8];

    // Lever/coucher soleil (UNIX timestamp, 0 si inconnu)
    uint32_t sunrise_ts;
    uint32_t sunset_ts;
};

// ─── Donnees actuelles (now) ──────────────────────────────────────────────────
struct WeatherCurrent {
    float    temp;
    float    feels_like;
    int      humidity;
    float    pressure_hpa;
    float    wind_speed_kmh;
    int      wind_deg;
    float    wind_gust_kmh;
    int      uv_index;
    int      owm_id;
    char     description[48];
    char     icon_lv[8];
    uint32_t sunrise_ts;
    uint32_t sunset_ts;
    bool     valid;
};

// ─── Conteneur global ─────────────────────────────────────────────────────────
struct WeatherData {
    WeatherDay     days[3];       // Previsions J, J+1, J+2
    WeatherCurrent current;       // Conditions actuelles
    bool           valid;
    uint32_t       last_update_ms;
};

extern WeatherData weatherData;

// ─── API publique ─────────────────────────────────────────────────────────────
void        weather_init();
void        weather_update();           // Appeler toutes les 10 min

const char* owm_id_to_icon(int id);
uint32_t    owm_id_to_color(int id);   // Couleur hex 0xRRGGBB selon meteo
const char* wind_deg_to_dir(int deg);  // "N", "NE", "E" ... "NW"
const char* format_sun_time(uint32_t ts, char* buf, size_t len); // "06:32"
