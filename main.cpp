// workspace_lvgl/src/main.cpp
// ESP32 + TFT ILI9341 + LVGL
// BLE : JKBMS (NimBLE actif) + Victron (publicites passives)
// WiFi : NTP + OpenWeatherMap
// WATCHDOG : TWDT 30 secondes

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "ble/jkbms_ble.h"
#include "ble/victron_ble.h"
#include "ble/heating_ble.h"
#include "weather/weather.h"
#include "water/water_level.h"

// UI LVGL
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/lvgl_anim_icons.h"
#include "gps/gps.h"

// Forward declaration
void van_anims_init(void);

// ─── Constants ──────────────────────────────────────────
#define WDT_TIMEOUT_SEC      30
#define WIFI_MAX_RETRIES     3

// ─── Timers ──────────────────────────────────────────────
static uint32_t t_weather_update = 0;
static uint32_t t_ntp_update = 0;
static uint32_t t_clock_update = 0;
static uint32_t t_ble_update = 0;
static uint8_t wifi_retry_count = 0;

WiFiUDP ntp_udp;
NTPClient ntp_client(ntp_udp, NTP_SERVER, NTP_GMT_OFFSET + NTP_DST_OFFSET);

// ─── WiFi (Amélioré avec retry) ─────────────────────────
static void wifi_connect() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Deja connecte");
        van_anim_set_wifi(true);
        return;
    }

    Serial.printf("[WiFi] Connexion a %s (tentative %d/%d)...\n", 
                  WIFI_SSID, wifi_retry_count + 1, WIFI_MAX_RETRIES);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    for (uint8_t i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        esp_task_wdt_reset();
        Serial.print(".");
    }

    bool ok = (WiFi.status() == WL_CONNECTED);

    if (ok) {
        wifi_retry_count = 0;
        Serial.printf("[WiFi] OK - IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifi_retry_count++;
        if (wifi_retry_count >= WIFI_MAX_RETRIES) {
            Serial.printf("[WiFi] ECHEF apres %d tentatives - Mode offline\n", wifi_retry_count);
        }
    }

    van_anim_set_wifi(ok);
}

// ─── Setup ────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("[GG_Dashboard] Demarrage...");

    // ✅ Watchdog
    Serial.println("[WDT] Initialisation Watchdog (30s)...");
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    Serial.println("[WDT] OK");

    // Relais OFF par defaut
    const uint8_t relay_pins[] = {
        RELAY_LIGHT1, RELAY_LIGHT2, RELAY_LIGHT3,
        RELAY_LIGHT4, RELAY_LIGHT5, RELAY_LIGHT6,
        RELAY_TV,
        RELAY_WATER_PUMP,
        RELAY_WATER_HEATER
    };

    for (auto p : relay_pins) {
        pinMode(p, OUTPUT);
        digitalWrite(p, RELAY_OFF);
    }

    // LVGL + TFT
    ui.begin();

    // BLE
    jkBms.begin();
    victronBle.begin();
    heatingBle.begin();
    van_anim_set_ble(false);

    // WiFi + NTP
    wifi_connect();
    ntp_client.begin();
    ntp_client.update();

    // Meteo initiale
    weather_init();
    weather_update();
    if (weatherData.valid) {
        van_anim_set_weather(0, owm_to_anim_code(weatherData.days[0].owm_id));
        van_anim_set_weather(1, owm_to_anim_code(weatherData.days[1].owm_id));
        van_anim_set_weather(2, owm_to_anim_code(weatherData.days[2].owm_id));
    }

    // GPS NEO-6M (UART2)
    gps_init();

    // Niveau eau
    water_level_init();

    // Animations
    van_anims_init();

    Serial.println("[GG_Dashboard] PRET - Watchdog ACTIF");
}

// ─── Loop ────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    lv_timer_handler();

    jkBms.update();
    victronBle.update();
    heatingBle.update();

    water_level_update();

    // Etat BLE -> animations
    if (now - t_ble_update >= UI_BLE_UPDATE_MS) {
        t_ble_update = now;
        bool ble_ok = jkBms.isConnected();
        van_anim_set_ble(ble_ok);

        if (ble_ok) {
            const JkBmsData& d = jkBms.getData();
            van_anim_set_battery_charging(d.battery_current > 0.0f);
        }

        if (ui.currentPage() == PAGE_BATTERY) page_battery_update();
        if (ui.currentPage() == PAGE_HOME) page_home_update();
    }

    // Horloge
    if (now - t_clock_update >= UI_CLOCK_UPDATE_MS) {
        t_clock_update = now;
        ntp_client.update();
        
        if (ui.currentPage() == PAGE_HOME) page_home_update();
        if (ui.currentPage() == PAGE_SYSTEM) page_system_update();
        page_heating_update();
        gps_update();
    }

    // Meteo (toutes les 10 min)
    if (now - t_weather_update >= UPDATE_PERIOD_WEATHER_MS) {
        t_weather_update = now;
        if (WiFi.status() == WL_CONNECTED) {
            weather_update();
            if (weatherData.valid) {
                van_anim_set_weather(0, owm_to_anim_code(weatherData.days[0].owm_id));
                van_anim_set_weather(1, owm_to_anim_code(weatherData.days[1].owm_id));
                van_anim_set_weather(2, owm_to_anim_code(weatherData.days[2].owm_id));
            }
        }
    }

    // Reconnexion WiFi + NTP
    if (now - t_ntp_update >= UPDATE_PERIOD_NTP_MS) {
        t_ntp_update = now;
        if (WiFi.status() != WL_CONNECTED) {
            wifi_connect();
        }
        van_anim_set_wifi(WiFi.status() == WL_CONNECTED);
    }

    // ✅ Reset Watchdog
    esp_task_wdt_reset();

    delay(5);
}