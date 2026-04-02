#pragma once
/*
 * water_level.h - Lecture niveaux eau CBE MT214/M (sondes SP5)
 * 2 reservoirs : eau propre + eau usee
 * Signal : 0-2.5V → ESP32 ADC (GPIO34/35)
 */
#include <Arduino.h>

struct WaterLevels {
    float fresh_pct;       // 0-100% eau propre
    float waste_pct;       // 0-100% eau usee
    float fresh_voltage;   // V brut sonde propre
    float waste_voltage;   // V brut sonde usee
    bool  fresh_low;       // Alerte niveau bas eau propre (<15%)
    bool  waste_high;      // Alerte niveau haut eau usee (>85%)
    bool  valid;
    uint32_t last_update_ms;
};

extern WaterLevels waterLevels;

void water_level_init();
void water_level_update();  // Appeler dans loop()
