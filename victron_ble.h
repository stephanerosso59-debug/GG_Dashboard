#pragma once
/*
 * victron_ble.h - Module Victron via BLE Advertisement (NimBLE)
 * Base : Fabian-Schmidt/esphome-victron_ble
 * Protocol : Victron "Instant readout" - AES-128-CTR chiffre
 *
 * Appareils geres :
 *   - SmartSolar MPPT  (CF:A0:F0:EF:C4:39)
 *   - SmartBMV 712     (C5:9F:63:D2:E7:45)
 *   - BatteryProtect   (E5:7A:2C:1C:D4:0C)
 *   - OrionSmart DC-DC (DE:5F:86:D7:15:E3)
 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../bluetooth_config.h"

// ============================================================
//  Donnees MPPT (Solar Charger - SmartSolar)
// ============================================================
struct VictronMpptData {
    float    pv_voltage;          // V - Tension panneau
    float    pv_current;          // A - Courant panneau
    float    pv_power;            // W - Puissance panneau
    float    battery_voltage;     // V
    float    battery_current;     // A
    float    charge_power;        // W
    float    yield_today;         // Wh - Production du jour
    float    yield_total;         // kWh - Production totale
    uint8_t  device_state;        // 0=Off, 2=Fault, 3=Bulk, 4=Absorption, 5=Float
    uint8_t  charger_error;       // 0=No error
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Donnees SmartShunt (courant/energie — meme MAC que OrionSmart)
// ============================================================
struct VictronSmartshuntData {
    float    current;             // A  (+ charge, - decharge)
    float    power;               // W
    float    consumed_ah;         // Ah consommes
    float    soc;                 // %  State of Charge
    float    time_to_go;          // minutes (-1 = non dispo)
    bool     alarm;
    uint16_t alarm_reason;
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Donnees SmartBMV 712 (Battery Monitor)
// ============================================================
struct VictronBmvData {
    // ── Mesures en temps reel ───────────────────────────────
    float    battery_voltage;     // V  (tension batterie)
    float    battery_current;     // A  (+ = charge, - = decharge)
    float    battery_power;       // W  (calcule : V x I)
    float    soc;                 // %  State of Charge (0-100)
    float    time_to_go;          // minutes restantes (-1 = non dispo)
    float    consumed_ah;         // Ah consommes depuis pleine charge

    // ── Tension auxiliaire / temperature ───────────────────
    float    aux_voltage;         // V  (midpoint ou starter battery)
    float    aux_temperature;     // °C (si sonde temp branchee)
    bool     aux_is_temperature;  // true = temp, false = tension aux

    // ── Historique BMV-712 ──────────────────────────────────
    uint32_t charge_cycles;       // Nombre de cycles de charge
    uint32_t full_charges;        // Nombre de charges completes
    float    deepest_discharge_ah;// Ah - decharge la plus profonde
    float    last_discharge_ah;   // Ah - derniere decharge
    float    average_discharge_ah;// Ah - decharge moyenne
    float    cumulative_ah;       // Ah - total cumule depuis reset

    // ── Etat relais & alarmes ───────────────────────────────
    bool     relay_state;         // true = relais ferme
    bool     alarm;               // true = alarme active
    uint16_t alarm_reason;        // Bits : 0=LowV 1=HighV 2=LowSOC ...

    // ── Tendance (calcule localement) ──────────────────────
    int8_t   trend;               // +1=charge -1=decharge 0=stable

    // ── Meta ────────────────────────────────────────────────
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// Alias de compatibilite (legacy)
typedef VictronBmvData VictronShuntData;

// ============================================================
//  Donnees BatteryProtect
// ============================================================
struct VictronBpData {
    float    input_voltage;       // V - Tension entree
    float    output_voltage;      // V - Tension sortie
    uint8_t  device_state;        // 0=Off, 1=On, 2=Fault ...
    bool     alarm;               // true = alarme active
    uint16_t alarm_reason;
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Donnees OrionSmart DC-DC
// ============================================================
struct VictronOrionData {
    float    input_voltage;       // V - Tension batterie vehicule
    float    output_voltage;      // V - Tension batterie van
    float    output_current;      // A - Courant de charge
    float    output_power;        // W
    uint8_t  device_state;        // 0=Off, 9=Charging, 11=Ext.control
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Structure interne du paquet Victron (non-chiffre)
// ============================================================
#pragma pack(push, 1)
struct VictronBleRecordBase {
    uint8_t  manufacturer_record_type;   // Doit etre 0x10
    uint8_t  unknown;
    uint16_t model_id;
    uint8_t  readout_type;
    uint16_t record_id;
    uint8_t  encryption_key_0;           // Premier octet du bindkey pour validation
};
#pragma pack(pop)

#define VICTRON_ENCRYPTED_DATA_MAX_SIZE  16

// ============================================================
//  Callbacks
// ============================================================
class VictronScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
};

// ============================================================
//  Classe VictronBle — 4 appareils
// ============================================================
class VictronBle {
public:
    VictronBle();
    void begin();
    void update();   // Appeler dans loop()

    const VictronMpptData&        getMpptData()       const { return _mppt;       }
    const VictronSmartshuntData&  getSmartshuntData() const { return _smartshunt; }
    const VictronBmvData&         getBmvData()        const { return _bmv;        }
    const VictronBpData&          getBpData()         const { return _bp;         }
    const VictronOrionData&       getOrionData()      const { return _orion;      }

    // Alias de compatibilite
    const VictronBmvData&         getShuntData()      const { return _bmv; }

    static VictronBle* instance;

    // Appele par le callback scan
    void onAdvertisement(NimBLEAdvertisedDevice* device);

private:
    enum DeviceType { DEV_MPPT, DEV_SMARTSHUNT, DEV_BMV, DEV_BP, DEV_ORION };

    void _parseAdvertisement(NimBLEAdvertisedDevice* device,
                             const uint8_t* bindkey, DeviceType type);
    bool _decrypt(const uint8_t* payload, size_t payloadLen,
                  const uint8_t* bindkey,
                  const uint8_t* mac,
                  uint16_t record_id,
                  uint8_t* outData, size_t outLen);
    void _hexToBytes(const char* hexStr, uint8_t* bytes, size_t len);
    void _parseSolarCharger(const uint8_t* data);
    void _parseSmartshunt(const uint8_t* data);
    void _parseBatteryMonitor(const uint8_t* data);
    void _parseBatteryProtect(const uint8_t* data);
    void _parseOrionSmart(const uint8_t* data);
    const char* _stateToString(uint8_t state);

    VictronMpptData       _mppt;
    VictronSmartshuntData _smartshunt;
    VictronBmvData        _bmv;
    VictronBpData         _bp;
    VictronOrionData      _orion;
    VictronScanCallbacks _scanCbs;
    uint32_t _lastScanMs;
};

extern VictronBle victronBle;
