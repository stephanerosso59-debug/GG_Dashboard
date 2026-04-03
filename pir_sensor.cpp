/*
 * pir_sensor.cpp - Implémentation capteur PIR HC-SR505
 * Auto-sleep écran TFT pour économie d'énergie
 */

#include "pir_sensor.h"

// ─── Variables internes ──────────────────────────────────────
static uint32_t last_motion_time = 0;
static ScreenState screen_state = SCREEN_STATE_ON;
static uint32_t screen_timeout = SCREEN_TIMEOUT_MS;

// Callbacks utilisateur
static PirScreenOnCallback cb_on = NULL;
static PirScreenOffCallback cb_off = NULL;

// Anti-rebond : ignore détections trop rapprochées
static uint32_t last_detect_time = 0;

// ─── Fonctions privées ──────────────────────────────────────────

static void turn_screen_on() {
    if (screen_state == SCREEN_STATE_ON) return;  // Déjà allumé
    
    Serial.println("[PIR] ➡️ Screen ON");
    
    // Activer le rétro-éclairage
    digitalWrite(BACKLIGHT_PIN, HIGH);
    
    // Réveiller LVGL si en mode sleep
    lv_disp_inv_ready(true);
    
    // Notifier le callback
    if (cb_on != NULL) {
        cb_on();
    }
    
    screen_state = SCREEN_STATE_ON;
}

static void turn_screen_off() {
    if (screen_state == SCREEN_STATE_OFF) return;  Déjà éteint
    
    Serial.println("[PIR] 💤 Screen OFF (sleep)");
    
    // Couper le rétro-éclairage
    digitalWrite(BACKLIGHT_PIN, LOW);
    
    // Mettre LVGL en mode basse consommation
    // L'écran reste affiché mais backlight off = pas de consommation visible
    // Pour économiser plus : lv_disp_off(NULL); // Coupe complètement
    
    // Notifier le callback
    if (cb_off != NULL) {
        cb_off();
    }
    
    screen_state = SCREEN_STATE_OFF;
}

// ─── Implémentations publiques ──────────────────────────────────

void pir_sensor_init(PirScreenOnCallback on_cb, PirScreenOffCallback off_cb) {
    // Sauvegarder callbacks
    cb_on = on_cb;
    cb_off = off_cb;
    
    // Configurer le GPIO du capteur comme entrée
    pinMode(PIR_SENSOR_PIN, INPUT);
    
    // Configurer le GPIO backlight comme sortie
    pinMode(BACKLIGHT_PIN, OUTPUT);
    
    // État initial : écran allumé
    turn_screen_on();
    last_motion_time = millis();
    
    Serial.printf("[PIR] Initialisé - GPIO: %d, Backlight: %d, Timeout: %lums\n",
                  PIR_SENSOR_PIN, BACKLIGHT_PIN, screen_timeout);
}

bool pir_sensor_update(void) {
    uint32_t now = millis();
    bool motion = (digitalRead(PIR_SENSOR_PIN) == HIGH);
    
    bool state_changed = false;
    
    // === CAS 1 : MOUVEMENT DÉTECTÉ ===
    if (motion) {
        // Anti-rebond : ignorer si détection trop proche
        if (now - last_detect_time < PIR_DEBOUNCE_MS) {
            return false;  // Trop tôt, ignorer
        }
        
        last_detect_time = now;
        last_motion_time = now;
        
        // Si écran était éteint → le rallumer
        if (screen_state != SCREEN_STATE_ON) {
            turn_screen_on();
            state_changed = true;
        }
    }
    
    // === CAS 2 : PAS DE MOUVEMENT - VÉRIFIER TIMEOUT ===
    else {
        if (screen_state == SCREEN_STATE_ON) {
            // Calculer temps depuis dernier mouvement
            uint32_t idle_time = now - last_motion_time;
            
            if (idle_time >= screen_timeout) {
                turn_screen_off();
                state_changed = true;
            }
        }
    }
    
    return state_changed;
}

void pir_sensor_wake_screen(void) {
    turn_screen_on();
    last_motion_time = millis();
}

ScreenState pir_get_screen_state(void) {
    return screen_state;
}

void pir_set_timeout(uint32_t timeout_ms) {
    screen_timeout = timeout_ms;
    Serial.printf("[PIR] Timeout mis à jour: %lus\n", timeout_ms);
}

bool pir_is_motion_detected(void) {
    return (digitalRead(PIR_SENSOR_PIN) == HIGH);
}