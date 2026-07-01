/*
 * water_state.cpp — État eau van (alimente par ESP-NOW depuis PCB ESP32-U)
 */
#include "water_state.h"
#include "espnow_master.h"

WaterState waterState = {};

void water_init() {
    waterState.clean_capacity_l = 100.0f;   // 100L eau propre (config user)
    waterState.dirty_capacity_l = 80.0f;    // 80L eau sale
    waterState.clean_pct = 0.0f;
    waterState.dirty_pct = 0.0f;
    waterState.pump_on = false;
    waterState.water_heater_on = false;
    waterState.water_temp_c = 18.0f;
    waterState.valid = false;
    waterState.last_update_ms = millis();
    Serial0.println("[Water] Init (en attente donnees ESP-NOW du PCB)");
}

void water_update() {
    // Reprend les donnees recues du slave PCB via ESP-NOW
    if (statusValid && (millis() - statusLastUpdateMs < 30000)) {
        // PCB en ligne et reponse fraiche
        waterState.clean_pct = statusFromSlave.water_clean_pct;
        waterState.dirty_pct = statusFromSlave.water_dirty_pct;
        waterState.clean_liters = waterState.clean_pct * waterState.clean_capacity_l / 100.0f;
        waterState.dirty_liters = waterState.dirty_pct * waterState.dirty_capacity_l / 100.0f;
        
        // Lire l'etat reel des relais 7 (pompe) et 8 (chauffe-eau)
        waterState.pump_on         = (statusFromSlave.relays_actual >> 6) & 1;
        waterState.water_heater_on = statusFromSlave.chauffe_eau_actual;
        
        waterState.valid = true;
        waterState.last_update_ms = millis();
    } else {
        // PCB pas vu depuis 30s -> donnees obsoletes
        waterState.valid = false;
    }
}

// Callbacks UI : commande la pompe ou le chauffe-eau via ESP-NOW
void water_set_pump(bool on) {
    static uint8_t current_mask = 0;
    
    // Reprendre l'etat actuel des relais (depuis status si dispo)
    if (statusValid) current_mask = statusFromSlave.relays_actual;
    
    // Modifier le bit 6 (relais 7)
    if (on) current_mask |=  (1 << 6);
    else    current_mask &= ~(1 << 6);
    
    // Chauffe-eau = champ separe : on preserve son etat courant
    uint8_t chauffe_eau = statusValid ? statusFromSlave.chauffe_eau_actual
                                      : (waterState.water_heater_on ? 1 : 0);
    espnow_send_relays(current_mask, chauffe_eau);

    // Mise a jour optimiste cote UI (le feedback viendra via status)
    waterState.pump_on = on;
}

void water_set_heater(bool on) {
    static uint8_t current_mask = 0;
    if (statusValid) current_mask = statusFromSlave.relays_actual;

    // Chauffe-eau = champ dedie (plus le bit 7 du bitmask, qui est "Divers")
    espnow_send_relays(current_mask, on ? 1 : 0);
    waterState.water_heater_on = on;
}
