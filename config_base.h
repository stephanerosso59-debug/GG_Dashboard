#pragma once
// config_base.h - Constantes communes aux 3 workspaces
// Editer ce fichier avec vos propres valeurs avant compilation

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID        "VotreSSID"
#define WIFI_PASSWORD    "VotreMotDePasse"

// ─── NTP ─────────────────────────────────────────────────────────────────────
#define NTP_SERVER       "pool.ntp.org"
#define NTP_GMT_OFFSET   3600      // UTC+1 (France hiver)
#define NTP_DST_OFFSET   3600      // +1h ete

// ─── OpenWeatherMap ──────────────────────────────────────────────────────────
#define OWM_API_KEY      "11370682e8ddbaefaab02d8878a744a2"
#define OWM_CITY         "Paris"
#define OWM_COUNTRY      "FR"
#define OWM_LANG         "fr"
#define OWM_UNITS        "metric"

// ─── BLE JKBMS ───────────────────────────────────────────────────────────────
// Toutes les constantes BLE sont dans bluetooth_config.h
#include "bluetooth_config.h"

// Nombre de cellules (8, 16, 24 ou 32)
#define JKBMS_CELL_COUNT 16

// ─── Relais GPIO lumieres ────────────────────────────────────────────────────
#define RELAY_LIGHT1    26      // Salon
#define RELAY_LIGHT2    27      // Cuisine
#define RELAY_LIGHT3    14      // Chambre
#define RELAY_LIGHT4    12      // WC
#define RELAY_LIGHT5    13      // Ext. Avant
#define RELAY_TV        33      // Television

// ─── Niveau logique relais (module AliExpress 5V actif BAS SRD-05VDC) ────────
// IN=LOW  (0V)   → relais fermé  (ON)
// IN=HIGH (3.3V) → relais ouvert (OFF)
#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

// ─── Chauffage ───────────────────────────────────────────────────────────────
// HEATING_RELAY_PIN supprimé — chauffage piloté via BLE (c1:02:29:4f:fe:50)
#define HEATING_TEMP_MIN    5.0f
#define HEATING_TEMP_MAX    30.0f
#define HEATING_TEMP_STEP   0.5f
#define HEATING_TEMP_DEFAULT 20.0f

// ─── Niveau Eau (CBE MT214/M + sondes SP5) ──────────────────────────────────
// Signal sonde : 0-2.5V proportionnel au niveau (capacitif)
// Cablage : Blanc=+5V  Marron=GND  Vert=Signal → ESP32 ADC
#define WATER_FRESH_ADC_PIN   34    // GPIO34 (ADC1_CH6) — eau propre
#define WATER_WASTE_ADC_PIN   35    // GPIO35 (ADC1_CH7) — eau usee
#define WATER_ADC_VREF        2.5f  // Tension max sonde = reservoir plein
#define WATER_ADC_RESOLUTION  4095  // ESP32 ADC 12 bits
#define WATER_UPDATE_MS       5000  // Intervalle lecture (ms)

// ─── Pompe eau / Chauffe-eau (relais) ────────────────────────────────────────
#define RELAY_WATER_PUMP    15      // GPIO15 — pompe eau propre
#define RELAY_WATER_HEATER  25      // GPIO25 — chauffe-eau (ex Ext. Arriere)

// ─── Periodes de mise a jour (ms) ────────────────────────────────────────────
#define UPDATE_PERIOD_BLE_MS     5000
#define UPDATE_PERIOD_WEATHER_MS 600000  // 10 min
#define UPDATE_PERIOD_NTP_MS     3600000 // 1 heure

// ─── GPS NEO-6M (UART2) ──────────────────────────────────────────────────────
#define GPS_RX_PIN          16      // GPIO16 (UART2 RX) ← TX du NEO-6M
#define GPS_TX_PIN          17      // GPIO17 (UART2 TX) → RX du NEO-6M
#define GPS_BAUD            9600
#define GPS_UPDATE_MS       1000    // Parsing GPS toutes les secondes
