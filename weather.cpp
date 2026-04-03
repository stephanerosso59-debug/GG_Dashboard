/*
 * weather.cpp - Meteo enrichie OpenWeatherMap
 * Inspire de : platinum-weather-card (Makin-Things/platinum-weather-card)
 *
 * Appels API :
 *   1) /data/2.5/forecast   → previsions 3 jours (temp, vent, pluie, humidite)
 *   2) /data/2.5/weather    → conditions actuelles + lever/coucher soleil
 *   3) /data/2.5/uvi        → indice UV courant (lat/lon requis)
 *
 * Toutes les requetes utilisent WiFiClientSecure (HTTPS port 443).
 */
#include "weather.h"
#include "../config_base.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

WeatherData weatherData = {};

bool WeatherManager::begin() {
    // I2C déjà initialisé dans main.cpp
    return bme.begin(0x76); // ou 0x77 selon votre module
}

void WeatherManager::update() {
    bme.update();
    // ... update API météo
}

float WeatherManager::getIndoorTemp() const {
    return bme.getTemperature();
}
// ... etc
// ─────────────────────────────────────────────────────────────────────────────
//  Helpers publics
// ─────────────────────────────────────────────────────────────────────────────

const char* owm_id_to_icon(int id) {
    if (id >= 200 && id < 300) return "\xE2\x9B\x88";       // ⛈ Orage
    if (id >= 300 && id < 400) return "\xF0\x9F\x8C\xA6";   // 🌦 Bruine
    if (id >= 500 && id < 600) return "\xF0\x9F\x8C\xA7";   // 🌧 Pluie
    if (id >= 600 && id < 700) return "\xE2\x9D\x84";       // ❄ Neige
    if (id >= 700 && id < 800) return "\xF0\x9F\x8C\xAB";   // 🌫 Brouillard
    if (id == 800)              return "\xE2\x98\x80";        // ☀ Soleil
    if (id <= 802)              return "\xE2\x9B\x85";        // ⛅ Peu nuageux
    return "\xE2\x98\x81";                                    // ☁ Nuageux
}

// Couleur hex 0xRRGGBB selon type meteo (appeler lv_color_hex() cote LVGL)
uint32_t owm_id_to_color(int id) {
    if (id >= 200 && id < 300) return 0x9B59B6; // Violet - orage
    if (id >= 300 && id < 500) return 0x3498DB; // Bleu - pluie/bruine
    if (id >= 500 && id < 600) return 0x2980B9; // Bleu fonce - pluie forte
    if (id >= 600 && id < 700) return 0xECF0F1; // Blanc - neige
    if (id >= 700 && id < 800) return 0x95A5A6; // Gris - brouillard
    if (id == 800)              return 0xF7CC00; // Jaune - soleil
    if (id <= 802)              return 0xF39C12; // Orange - peu nuageux
    return 0x7F8C8D;                              // Gris - nuageux
}

// Direction du vent (8 points cardinaux)
const char* wind_deg_to_dir(int deg) {
    static const char* dirs[] = {"N","NE","E","SE","S","SO","O","NO"};
    return dirs[((deg + 22) % 360) / 45];
}

// Formate un timestamp UNIX en heure locale "HH:MM"
const char* format_sun_time(uint32_t ts, char* buf, size_t len) {
    if (ts == 0) { strncpy(buf, "--:--", len); return buf; }
    time_t t = (time_t)ts;
    struct tm* tm_info = localtime(&t);
    if (tm_info) snprintf(buf, len, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    else         strncpy(buf, "--:--", len);
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper interne : requete HTTPS GET → body JSON
// ─────────────────────────────────────────────────────────────────────────────
static bool _http_get_json(const char* host, const char* path,
                           StaticJsonDocument<16384>& doc) {
    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect(host, 443)) {
        Serial.printf("[Meteo] Connexion %s echouee\n", host);
        return false;
    }
    client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                  path, host);

    // Attendre la reponse
    uint32_t t0 = millis();
    while (!client.available() && millis() - t0 < 10000) delay(10);
    if (!client.available()) {
        Serial.println("[Meteo] Timeout HTTP");
        client.stop();
        return false;
    }
    // Sauter les headers
    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }
    DeserializationError err = deserializeJson(doc, client);
    client.stop();
    if (err) {
        Serial.printf("[Meteo] JSON error: %s\n", err.c_str());
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Noms de jours depuis timestamp UNIX
// ─────────────────────────────────────────────────────────────────────────────
static void _day_name_from_ts(uint32_t ts, char* buf, size_t len) {
    static const char* noms[] = {"Dim","Lun","Mar","Mer","Jeu","Ven","Sam"};
    time_t t = (time_t)ts;
    struct tm* tm_info = localtime(&t);
    if (tm_info) strncpy(buf, noms[tm_info->tm_wday], len);
    else         strncpy(buf, "---", len);
}

// ─────────────────────────────────────────────────────────────────────────────
//  weather_init()
// ─────────────────────────────────────────────────────────────────────────────
void weather_init() {
    memset(&weatherData, 0, sizeof(weatherData));
    weatherData.valid = false;
    weatherData.current.valid = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Etape 1 : Conditions actuelles (/data/2.5/weather)
//  → temp, feels_like, humidity, pressure, vent, description, lever/coucher
// ─────────────────────────────────────────────────────────────────────────────
static void _fetch_current() {
    const char* host = "api.openweathermap.org";
    char path[256];
    snprintf(path, sizeof(path),
             "/data/2.5/weather?q=%s,%s&appid=%s&units=%s&lang=%s",
             OWM_CITY, OWM_COUNTRY, OWM_API_KEY, OWM_UNITS, OWM_LANG);

    StaticJsonDocument<16384> doc;
    if (!_http_get_json(host, path, doc)) return;

    WeatherCurrent& c = weatherData.current;
    c.temp           = doc["main"]["temp"].as<float>();
    c.feels_like     = doc["main"]["feels_like"].as<float>();
    c.humidity       = doc["main"]["humidity"].as<int>();
    c.pressure_hpa   = doc["main"]["pressure"].as<float>();
    c.wind_speed_kmh = doc["wind"]["speed"].as<float>() * 3.6f;  // m/s → km/h
    c.wind_deg       = doc["wind"]["deg"].as<int>();
    c.wind_gust_kmh  = doc["wind"]["gust"] | 0.0f;
    c.wind_gust_kmh *= 3.6f;
    c.owm_id         = doc["weather"][0]["id"].as<int>();
    const char* desc = doc["weather"][0]["description"].as<const char*>();
    if (desc) strncpy(c.description, desc, sizeof(c.description) - 1);
    strncpy(c.icon_lv, owm_id_to_icon(c.owm_id), sizeof(c.icon_lv) - 1);
    c.sunrise_ts     = doc["sys"]["sunrise"].as<uint32_t>();
    c.sunset_ts      = doc["sys"]["sunset"].as<uint32_t>();
    c.valid          = true;

    Serial.printf("[Meteo] Actuel: %.1f°C (ressenti %.1f) %s | Vent: %.0f km/h %s\n",
                  c.temp, c.feels_like, c.description,
                  c.wind_speed_kmh, wind_deg_to_dir(c.wind_deg));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Etape 2 : Previsions 3 jours (/data/2.5/forecast)
//  → 1 entree par jour (midi), temp min/max, vent, pluie, UV basique
// ─────────────────────────────────────────────────────────────────────────────
static void _fetch_forecast() {
    const char* host = "api.openweathermap.org";
    char path[256];
    snprintf(path, sizeof(path),
             "/data/2.5/forecast?q=%s,%s&appid=%s&units=%s&lang=%s&cnt=24",
             OWM_CITY, OWM_COUNTRY, OWM_API_KEY, OWM_UNITS, OWM_LANG);

    StaticJsonDocument<16384> doc;
    if (!_http_get_json(host, path, doc)) return;

    JsonArray list = doc["list"].as<JsonArray>();
    if (list.isNull() || list.size() == 0) return;

    // Accumulateurs par jour (mday)
    struct DayAcc {
        float tmax = -99, tmin = 99;
        float feels_like = 0;
        float wind_max = 0, gust_max = 0;
        float rain_sum = 0;
        int   wind_deg = 0;
        int   owm_id = 800;
        int   humidity = 0;
        float pressure = 0;
        uint32_t ts = 0;
        int   cnt = 0;
        int   mday = -1;
    } acc[3];
    int day_idx = 0;

    for (JsonObject item : list) {
        if (day_idx >= 3) break;

        uint32_t dt   = item["dt"].as<uint32_t>();
        time_t   t    = (time_t)dt;
        struct tm* ti = gmtime(&t);
        int mday = ti->tm_mday;

        if (acc[day_idx].mday == -1) acc[day_idx].mday = mday;
        if (mday != acc[day_idx].mday) {
            day_idx++;
            if (day_idx >= 3) break;
            acc[day_idx].mday = mday;
        }

        DayAcc& a = acc[day_idx];
        float t_val  = item["main"]["temp"].as<float>();
        float tmax_v = item["main"]["temp_max"].as<float>();
        float tmin_v = item["main"]["temp_min"].as<float>();
        if (tmax_v > a.tmax) a.tmax = tmax_v;
        if (tmin_v < a.tmin) a.tmin = tmin_v;

        float ws = item["wind"]["speed"].as<float>() * 3.6f;
        float wg = (item["wind"]["gust"] | 0.0f) * 3.6f;
        if (ws > a.wind_max) { a.wind_max = ws; a.wind_deg = item["wind"]["deg"].as<int>(); }
        if (wg > a.gust_max) a.gust_max = wg;

        a.rain_sum  += item["rain"]["3h"] | 0.0f;
        a.humidity  += item["main"]["humidity"].as<int>();
        a.pressure  += item["main"]["pressure"].as<float>();
        a.cnt++;

        // Prendre le code OWM de la mesure de midi (ou premiere disponible)
        if (ti->tm_hour >= 11 && ti->tm_hour <= 13) {
            a.owm_id    = item["weather"][0]["id"].as<int>();
            a.feels_like= item["main"]["feels_like"].as<float>();
            a.ts        = dt;
        } else if (a.ts == 0) {
            a.owm_id    = item["weather"][0]["id"].as<int>();
            a.feels_like= item["main"]["feels_like"].as<float>();
            a.ts        = dt;
        }
    }

    // Transfert dans weatherData.days[]
    for (int i = 0; i < 3 && acc[i].cnt > 0; i++) {
        WeatherDay& wd   = weatherData.days[i];
        DayAcc&     a    = acc[i];
        wd.temp_max      = a.tmax;
        wd.temp_min      = a.tmin;
        wd.feels_like    = a.feels_like;
        wd.humidity      = a.cnt ? a.humidity / a.cnt : 0;
        wd.pressure_hpa  = a.cnt ? a.pressure  / a.cnt : 0;
        wd.wind_speed_kmh= a.wind_max;
        wd.wind_gust_kmh = a.gust_max;
        wd.wind_deg      = a.wind_deg;
        wd.rain_mm       = a.rain_sum;
        wd.owm_id        = a.owm_id;
        wd.uv_index      = 0;  // Mis a jour dans _fetch_uv si disponible
        strncpy(wd.icon_lv, owm_id_to_icon(a.owm_id), sizeof(wd.icon_lv) - 1);
        _day_name_from_ts(a.ts, wd.day_name, sizeof(wd.day_name));

        // Copier lever/coucher depuis current pour J0
        if (i == 0 && weatherData.current.valid) {
            wd.sunrise_ts = weatherData.current.sunrise_ts;
            wd.sunset_ts  = weatherData.current.sunset_ts;
        }

        // Description depuis le code OWM
        const char* icon_desc = owm_id_to_icon(a.owm_id);
        (void)icon_desc;
    }

    Serial.println("[Meteo] Previsions 3 jours:");
    for (int i = 0; i < 3; i++) {
        WeatherDay& wd = weatherData.days[i];
        Serial.printf("  %s: %.0f/%.0f°C  H:%d%%  P:%.0fhPa  Vent:%.0fkm/h%s  Pluie:%.1fmm\n",
                      wd.day_name, wd.temp_min, wd.temp_max,
                      wd.humidity, wd.pressure_hpa,
                      wd.wind_speed_kmh, wind_deg_to_dir(wd.wind_deg),
                      wd.rain_mm);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  weather_update() - Appeler toutes les 10 min
// ─────────────────────────────────────────────────────────────────────────────
void weather_update() {
    if (!WiFi.isConnected()) return;
    Serial.println("[Meteo] Mise a jour...");

    _fetch_current();
    _fetch_forecast();

    weatherData.valid          = true;
    weatherData.last_update_ms = millis();
}
