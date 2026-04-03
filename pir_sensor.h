/*
 * pir_sensor.h - Gestionnaire capteur PIR HC-SR505
 * Auto-allumage/extinction écran TFT
 */

#pragma once

#include <Arduino.h>

// ─── Configuration ────────────────────────────────────────

/** GPIO du capteur PIR (modifier selon votre câblage) */
#ifndef PIR_SENSOR_PIN
#define PIR_SENSOR_PIN      4
#endif

/** GPIO contrôle backlight (via transistor NPN ou MOSFET) */
#ifndef BACKLIGHT_PIN
#define BACKLIGHT_PIN       27
#endif

/** Temps avant extinction écran (millisecondes) */
#ifndef SCREEN_TIMEOUT_MS
#define SCREEN_TIMEOUT_MS   (30UL * 1000UL)  // 30 secondes par défaut
#endif

/** Intervalle de vérification du capteur (ms) */
#define PIR_CHECK_INTERVAL_MS  200

/** Délai minimum entre deux détections (anti-rebond) */
#define PIR_DEBOUNCE_MS         2000

// ─── États du système ──────────────────────────────────────
typedef enum {
    SCREEN_STATE_ON = 0,     // Écran allumé, LVGL actif
    SCREEN_STATE_OFF = 1,    // Écran éteint, économie d'énergie
    SCREEN_STATE_WAKING = 2   // Transition ON (réveil)
} ScreenState;

// ─── Callbacks optionnels ──────────────────────────────────
typedef void (*PirScreenOnCallback)(void);
typedef void (*PirScreenOffCallback)(void);

// ─── Fonctions publiques ────────────────────────────────────

/**
 * @brief Initialiser le capteur PIR et le contrôle backlight
 * @param on_cb  Callback appelé quand écran s'allume (peut être NULL)
 * @param off_cb Callback appelé quand écran s'éteint (peut être NULL)
 */
void pir_sensor_init(PirScreenOnCallback on_cb = NULL, 
                     PirScreenOffCallback off_cb = NULL);

/**
 * @brief Mettre à jour le capteur (à appeler dans loop())
 * @return true si l'état de l'écran a changé
 */
bool pir_sensor_update(void);

/**
 * @brief Forcer l'écran allumé (réinitialise le timer)
 */
void pir_sensor_wake_screen(void);

/**
 * @brief Obtenir l'état actuel de l'écran
 */
ScreenState pir_get_screen_state(void);

/**
 * @brief Modifier le timeout dynamiquement
 * @param timeout_ms Nouveau timeout en millisecondes
 */
void pir_set_timeout(uint32_t timeout_ms);

/**
 * @brief Vérifier si mouvement détecté maintenant
 */
bool pir_is_motion_detected(void);