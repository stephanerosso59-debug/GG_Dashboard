#pragma once
// workspace_lvgl/src/config.h
// Configuration specifique au workspace LVGL (TFT ILI9341)
// Les constantes communes (WiFi, BLE, relais) sont dans shared/config_base.h

#include "../../shared/config_base.h"

// ─── Ecran TFT ILI9341 ───────────────────────────────────────────────────────
#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   320
#define SCREEN_ROTATION 1     // 0=portrait, 1=paysage

// ─── LVGL ────────────────────────────────────────────────────────────────────
// Buffer = LV_BUF_LINES * largeur * 2 octets = 20*480*2 = ~19 KB
// Defini via build_flags -DLV_BUF_LINES=20

// ─── SPI TFT (redondant avec build_flags, pour reference) ───────────────────
#define PIN_TFT_MISO    19
#define PIN_TFT_MOSI    23
#define PIN_TFT_SCLK    18
#define PIN_TFT_CS       5
#define PIN_TFT_DC       2
#define PIN_TFT_RST     -1      // Reset gere par EN ou non connecte
#define PIN_TOUCH_CS    -1      // Pas de touch — GPIO15 utilise pour pompe eau

// ─── Options UI LVGL ─────────────────────────────────────────────────────────
#define UI_ANIM_WIFI_PERIOD_MS    120
#define UI_ANIM_BLE_PERIOD_MS     100
#define UI_ANIM_WEATHER_PERIOD_MS 150
#define UI_ANIM_HEAT_PERIOD_MS    100
#define UI_CLOCK_UPDATE_MS        1000
#define UI_BLE_UPDATE_MS          2000
