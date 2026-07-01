#pragma once
/*
 * victron_ble.h — Décodage advertisements BLE Victron (MPPT, BMV, BP, Orion)
 * 
 * Basé sur la spec "Victron Instant Readout" (Victron fournit la doc PDF officielle)
 * Algo : AES-CTR avec bindkey 16 bytes
 * 
 * Les appareils Victron envoient des advertisements BLE **sans connexion GATT**
 * Il suffit de scanner passivement (déjà fait par ble_scanner).
 * Le ble_scanner appelle victron_on_advertisement() sur chaque pub manufacturer_id=0x02E1
 */
#include <Arduino.h>
#include <stdint.h>

// ═══ SmartSolar MPPT ═══
struct VictronMpptData {
    bool     valid;
    uint32_t last_update_ms;
    float    voltage_bat;       // V tension batterie sortie
    float    voltage_pv;        // V tension panneau PV
    float    current;           // A courant de charge
    float    power_pv_w;        // W puissance instantanée
    float    yield_today_wh;    // Wh cumulés aujourd'hui
    uint8_t  device_state;      // 0=off, 3=bulk, 4=absorb, 5=float, 7=equalize
    uint8_t  charger_error;
};

// ═══ SmartBMV 712 ═══
struct VictronBmvData {
    bool     valid;
    uint32_t last_update_ms;
    float    voltage;           // V batterie
    float    current;           // A (+ charge, - décharge)
    float    power_w;           // W instantanés
    float    soc_pct;           // 0-100 %
    float    consumed_ah;       // Ah consommés (négatif si vidé)
    uint16_t time_to_go_min;    // minutes restantes
    int16_t  aux_voltage_mv;    // tension auxiliaire / batterie de démarrage
    float    temperature_c;
};

// ═══ BatteryProtect ═══
struct VictronBpData {
    bool     valid;
    uint32_t last_update_ms;
    float    voltage;
    uint8_t  state;             // 0=init, 1=normal, 2=disconnected, 3=shutdown
    uint8_t  output_state;      // 0=off, 1=on
};

// ═══ OrionSmart DC/DC (même MAC que SmartShunt parfois) ═══
struct VictronOrionData {
    bool     valid;
    uint32_t last_update_ms;
    float    input_voltage;
    float    output_voltage;
    uint8_t  device_state;
    uint8_t  charger_error;
};

extern VictronMpptData  victronMpptData;
extern VictronBmvData   victronBmvData;
extern VictronBpData    victronBpData;
extern VictronOrionData victronOrionData;

void victron_init();

// Appelé par ble_scanner sur chaque advertisement manufacturer_id=0x02E1
// mac : adresse MAC (lowercase, format "xx:xx:xx:xx:xx:xx")
// data : payload manufacturer (sans les 2 bytes d'ID)
// len : longueur payload
void victron_on_advertisement(const char* mac, const uint8_t* data, size_t len);

bool victron_mppt_available();
bool victron_bmv_available();
