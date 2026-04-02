/*
 * water_level.cpp - Lecture ADC sondes CBE SP5
 * Sonde capacitive : 0V=vide, 2.5V=plein
 * Moyenne glissante sur 8 echantillons pour lisser le bruit ADC
 */
#include "water_level.h"
#include "../config_base.h"

WaterLevels waterLevels = {};

#define AVG_SAMPLES 8
static uint16_t fresh_buf[AVG_SAMPLES] = {};
static uint16_t waste_buf[AVG_SAMPLES] = {};
static uint8_t  buf_idx = 0;

// Convertir ADC brut en pourcentage (0-100%)
static float adc_to_pct(uint16_t raw) {
    float v = (float)raw / WATER_ADC_RESOLUTION * 3.3f;  // ESP32 Vref=3.3V
    float pct = (v / WATER_ADC_VREF) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static float adc_to_voltage(uint16_t raw) {
    return (float)raw / WATER_ADC_RESOLUTION * 3.3f;
}

static uint16_t avg_buf(const uint16_t* buf) {
    uint32_t sum = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) sum += buf[i];
    return (uint16_t)(sum / AVG_SAMPLES);
}

void water_level_init() {
    memset(&waterLevels, 0, sizeof(waterLevels));
    // GPIO34 et GPIO35 sont input-only sur ESP32, pas besoin de pinMode
    // Mais on configure l'attenuation ADC pour lire 0-3.3V
    analogSetAttenuation(ADC_11db);  // Plage 0-3.3V
    analogReadResolution(12);         // 12 bits (0-4095)

    // Remplir le buffer initial
    uint16_t f = analogRead(WATER_FRESH_ADC_PIN);
    uint16_t w = analogRead(WATER_WASTE_ADC_PIN);
    for (int i = 0; i < AVG_SAMPLES; i++) {
        fresh_buf[i] = f;
        waste_buf[i] = w;
    }

    Serial.printf("[Eau] Init OK - Fresh GPIO%d, Waste GPIO%d\n",
                  WATER_FRESH_ADC_PIN, WATER_WASTE_ADC_PIN);
}

void water_level_update() {
    static uint32_t lastMs = 0;
    if (millis() - lastMs < WATER_UPDATE_MS) return;
    lastMs = millis();

    // Lire ADC et stocker dans buffer circulaire
    fresh_buf[buf_idx % AVG_SAMPLES] = analogRead(WATER_FRESH_ADC_PIN);
    waste_buf[buf_idx % AVG_SAMPLES] = analogRead(WATER_WASTE_ADC_PIN);
    buf_idx++;

    // Moyennes
    uint16_t f_avg = avg_buf(fresh_buf);
    uint16_t w_avg = avg_buf(waste_buf);

    waterLevels.fresh_pct     = adc_to_pct(f_avg);
    waterLevels.waste_pct     = adc_to_pct(w_avg);
    waterLevels.fresh_voltage = adc_to_voltage(f_avg);
    waterLevels.waste_voltage = adc_to_voltage(w_avg);
    waterLevels.fresh_low     = (waterLevels.fresh_pct < 15.0f);
    waterLevels.waste_high    = (waterLevels.waste_pct > 85.0f);
    waterLevels.valid         = true;
    waterLevels.last_update_ms = millis();

    Serial.printf("[Eau] Propre: %.0f%% (%.2fV) | Usee: %.0f%% (%.2fV)%s%s\n",
                  waterLevels.fresh_pct, waterLevels.fresh_voltage,
                  waterLevels.waste_pct, waterLevels.waste_voltage,
                  waterLevels.fresh_low  ? " !! BAS" : "",
                  waterLevels.waste_high ? " !! PLEIN" : "");
}
