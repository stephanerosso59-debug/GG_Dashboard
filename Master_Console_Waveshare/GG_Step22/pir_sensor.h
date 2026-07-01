#pragma once
/*
 * pir_sensor.h — Détection présence PIR + extinction écran
 * 
 * GPIO 6 (connecteur ADC/Sensor PH2.0 du Waveshare)
 * Le PIR est en entrée digitale (HIGH = mouvement détecté)
 * 
 * Logique :
 *  - Si mouvement → rétro-éclairage ON, reset timer
 *  - Si pas de mouvement pendant DISPLAY_TIMEOUT_MS → éteindre
 */
#include <stdint.h>
#include <stdbool.h>

#define PIR_GPIO                 6
#define DISPLAY_TIMEOUT_MS       30000UL    // 30s pour test, mettre 300000 (5 min) en prod

extern bool pirLastState;       // true = mouvement détecté maintenant
extern bool displayOn;          // true = rétro-éclairage ON
extern uint32_t lastMotionMs;   // dernier mouvement détecté

void pir_init();
void pir_update();              // À appeler dans loop()
void display_force_on();        // Rallume (appel touch/manual)
