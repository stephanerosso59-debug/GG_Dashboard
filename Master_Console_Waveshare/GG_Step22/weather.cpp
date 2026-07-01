/*
 * weather.cpp — OpenWeatherMap forecast 3 jours
 */
#include "weather.h"
#include "config_base.h"
#include "user_config.h"
#include "gps.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherData weatherData = {};

static uint32_t lastCallMs = 0;
static const uint32_t CALL_INTERVAL_MS = 600000;  // 10 minutes

static void fillDay(WeatherDay& d, JsonObject obj, const char* label) {
    strncpy(d.label, label, sizeof(d.label)-1);
    d.label[sizeof(d.label)-1] = 0;
    d.temp_c       = obj["main"]["temp"]     | 0.0f;
    d.humidity_pct = obj["main"]["humidity"] | 0;
    d.wind_kmh     = ((float)(obj["wind"]["speed"] | 0.0f)) * 3.6f;
    const char* desc = obj["weather"][0]["description"] | "-";
    strncpy(d.condition, desc, sizeof(d.condition)-1);
    d.condition[sizeof(d.condition)-1] = 0;
    const char* ic = obj["weather"][0]["icon"] | "01d";
    strncpy(d.icon, ic, sizeof(d.icon)-1);
    d.icon[sizeof(d.icon)-1] = 0;
}

void weather_update() {
    uint32_t now = millis();
    // Premier appel immédiat, puis toutes 10 min
    if (weatherData.valid && (now - lastCallMs < CALL_INTERVAL_MS)) return;
    if (WiFi.status() != WL_CONNECTED) return;
    lastCallMs = now;
    
    char url[256];
    snprintf(url, sizeof(url),
        "http://api.openweathermap.org/data/2.5/forecast?q=%s&appid=%s&units=%s&lang=%s&cnt=24",
        userConfig.owm_city, userConfig.owm_api_key, OWM_UNITS, OWM_LANG);
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    
    int code = http.GET();
    if (code != 200) {
        Serial0.printf("[Weather] HTTP %d\n", code);
        http.end();
        return;
    }
    
    // Réponse volumineuse — filtrage JSON pour économiser RAM
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    
    if (err) {
        Serial0.printf("[Weather] JSON parse: %s\n", err.c_str());
        return;
    }
    
    JsonArray list = doc["list"];
    if (list.isNull() || list.size() < 1) {
        Serial0.println("[Weather] liste vide");
        return;
    }
    
    // AUJ = premier slot disponible
    fillDay(weatherData.today, list[0], "AUJ");
    
    // DEM = +24h (index 8 car slots de 3h)
    if (list.size() > 8) {
        fillDay(weatherData.tomorrow, list[8], "DEM");
    }
    
    // APR = +48h (index 16)
    if (list.size() > 16) {
        fillDay(weatherData.after, list[16], "APR");
    }
    
    weatherData.valid = true;
    weatherData.lastUpdate = now;
    Serial0.printf("[Weather] OK — AUJ %.1f°C, DEM %.1f°C, APR %.1f°C\n",
        weatherData.today.temp_c,
        weatherData.tomorrow.temp_c,
        weatherData.after.temp_c);
}

// ────────────────────────────────────────────────────────────────
// Open-Meteo : 7 jours de previsions (gratuit, sans cle)
// API : https://api.open-meteo.com/v1/forecast
// ────────────────────────────────────────────────────────────────

// Mapping codes WMO Open-Meteo -> codes OWM (compatibles avec icones SD)
//   0   = Ciel clair        -> 01d
//   1,2 = Peu nuageux        -> 02d
//   3   = Nuageux            -> 03d (ou 04d)
//   45,48 = Brouillard       -> 50d
//   51,53,55,61,63,65 = Pluie -> 10d
//   66,67 = Pluie verglacante -> 13d
//   71,73,75,77 = Neige      -> 13d
//   80,81,82 = Averses        -> 09d
//   95,96,99 = Orage          -> 11d
static const char* wmo_to_owm(int code) {
    if (code == 0) return "01d";
    if (code <= 2) return "02d";
    if (code == 3) return "04d";
    if (code == 45 || code == 48) return "50d";
    if (code >= 51 && code <= 65) return "10d";
    if (code >= 66 && code <= 67) return "13d";
    if (code >= 71 && code <= 77) return "13d";
    if (code >= 80 && code <= 82) return "09d";
    if (code >= 95 && code <= 99) return "11d";
    return "01d";  // fallback ciel clair
}

// Description francaise courte du code WMO
static const char* wmo_to_label(int code) {
    if (code == 0) return "Ensoleille";
    if (code == 1) return "Peu nuageux";
    if (code == 2) return "Partiellement";
    if (code == 3) return "Couvert";
    if (code == 45 || code == 48) return "Brouillard";
    if (code == 51) return "Bruine legere";
    if (code == 53) return "Bruine";
    if (code == 55) return "Bruine forte";
    if (code == 61) return "Pluie legere";
    if (code == 63) return "Pluie";
    if (code == 65) return "Pluie forte";
    if (code == 66 || code == 67) return "Pluie verglas";
    if (code >= 71 && code <= 75) return "Neige";
    if (code == 77) return "Neige granules";
    if (code == 80) return "Averses leg.";
    if (code == 81) return "Averses";
    if (code == 82) return "Averses fortes";
    if (code == 95) return "Orage";
    if (code == 96 || code == 99) return "Orage grele";
    return "Inconnu";
}

void weather_update_7days() {
    static uint32_t lastCallMs = 0;
    uint32_t now = millis();
    
    // Appel toutes 30 min (mise a jour moins frequente que OWM)
    if (weatherData.valid7days && (now - lastCallMs < 1800000UL)) return;
    if (WiFi.status() != WL_CONNECTED) return;
    lastCallMs = now;
    
    // Coordonnees : on prend Dijon par defaut (chez user)
    // Si GPS valide, on utilise la position actuelle
    extern struct GpsData gpsData;
    float lat = 47.32f, lon = 5.04f;  // Dijon par defaut
    bool from_gps = false;
    if (gpsData.has_fix &&
        gpsData.latitude > -90.0 && gpsData.latitude < 90.0 &&
        gpsData.longitude > -180.0 && gpsData.longitude < 180.0) {
        lat = gpsData.latitude;
        lon = gpsData.longitude;
        from_gps = true;
    }
    
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%.4f&longitude=%.4f"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max"
        "&forecast_days=7&timezone=Europe%%2FParis",
        lat, lon);
    
    Serial0.printf("[Weather7] GET (lat=%.4f lon=%.4f, %s) %s\n",
                   lat, lon, from_gps ? "GPS" : "Dijon defaut", url);
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();
    
    if (code != 200) {
        Serial0.printf("[Weather7] HTTP %d\n", code);
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    StaticJsonDocument<2048> filter;
    filter["daily"]["time"] = true;
    filter["daily"]["weather_code"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;
    filter["daily"]["precipitation_sum"] = true;
    filter["daily"]["wind_speed_10m_max"] = true;
    
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, payload,
                                                DeserializationOption::Filter(filter));
    if (err) {
        Serial0.printf("[Weather7] JSON err: %s\n", err.c_str());
        return;
    }
    
    JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
    JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
    JsonArray rain = doc["daily"]["precipitation_sum"].as<JsonArray>();
    JsonArray wind = doc["daily"]["wind_speed_10m_max"].as<JsonArray>();
    
    if (codes.size() < 7) {
        Serial0.println("[Weather7] Donnees incompletes");
        return;
    }
    
    static const char* DAY_LABELS[] = {"AUJ", "DEM", "APR", "+3", "+4", "+5", "+6"};
    
    for (int i = 0; i < 7; i++) {
        WeatherDay& d = weatherData.days7[i];
        strncpy(d.label, DAY_LABELS[i], sizeof(d.label));
        d.label[sizeof(d.label)-1] = 0;
        
        int wcode = codes[i].as<int>();
        d.temp_max = tmax[i].as<float>();
        d.temp_min = tmin[i].as<float>();
        d.temp_c = (d.temp_max + d.temp_min) / 2.0f;  // moyenne pour temp_c
        d.rain_mm = rain[i].as<float>();
        d.wind_kmh = wind[i].as<float>();
        d.humidity_pct = 0;  // pas dans cette API daily
        
        strncpy(d.icon, wmo_to_owm(wcode), sizeof(d.icon));
        d.icon[sizeof(d.icon)-1] = 0;
        
        const char* lbl = wmo_to_label(wcode);
        strncpy(d.condition, lbl, sizeof(d.condition));
        d.condition[sizeof(d.condition)-1] = 0;
    }
    
    weatherData.valid7days = true;
    Serial0.printf("[Weather7] OK - 7 jours: ");
    for (int i = 0; i < 7; i++) {
        Serial0.printf("%s=%.0f° ", weatherData.days7[i].label, weatherData.days7[i].temp_max);
    }
    Serial0.println();
}
