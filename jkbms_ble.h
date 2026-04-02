#pragma once
/*
 * jkbms_ble.h - Module JKBMS via BLE (NimBLE-Arduino)
 * Base : peff74/Arduino-jk-bms + syssi/esphome-jk-bms
 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../bluetooth_config.h"

// ============================================================
//  Structures de donnees JKBMS
// ============================================================
struct JkBmsCellData {
    float    cell_voltage[JKBMS_MAX_CELLS];  // Tension par cellule (V)
    uint8_t  cell_count;
    float    cell_voltage_min;               // Cellule la plus basse (V)
    float    cell_voltage_max;               // Cellule la plus haute (V)
    float    cell_voltage_avg;               // Tension moyenne (V)
    float    cell_delta;                     // Ecart max-min (mV)
    uint8_t  cell_min_index;
    uint8_t  cell_max_index;
};

struct JkBmsData {
    // Cellules
    JkBmsCellData cells;

    // Batterie
    float    battery_voltage;     // V
    float    battery_current;     // A (positif = charge)
    float    battery_power;       // W
    float    battery_soc;         // % (State of Charge)
    float    battery_capacity_ah; // Ah restant
    float    capacity_nominal_ah; // Ah nominal

    // Temperatures
    float    temp_mos;            // Temperature MOS (°C)
    float    temp_sensor1;        // Temperature sonde 1 (°C)
    float    temp_sensor2;        // Temperature sonde 2 (°C)

    // Status
    bool     charging_enabled;
    bool     discharging_enabled;
    bool     balancing_active;
    bool     charging_fet_on;
    bool     discharging_fet_on;

    // Protections
    bool     overvoltage_protection;
    bool     undervoltage_protection;
    bool     overcurrent_protection;
    bool     overtemp_protection;
    bool     short_circuit_protection;

    // Cycles
    uint32_t cycle_count;
    float    total_capacity_ah;   // Capacite totale chargee (Ah)

    // Etat connexion
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Callbacks NimBLE
// ============================================================
class JkBmsClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
};

class JkBmsScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
};

// ============================================================
//  Classe principale JKBMS
// ============================================================
class JkBmsBle {
public:
    JkBmsBle();

    void begin();
    void update();           // Appeler regulierement dans loop()

    const JkBmsData& getData() const { return _data; }
    bool isConnected() const { return _data.connected; }

    static JkBmsBle* instance;  // Singleton pour les callbacks

    friend class JkBmsClientCallbacks;
    friend class JkBmsScanCallbacks;

private:
    void _startScan();
    void _connect(NimBLEAdvertisedDevice* device);
    void _sendCommand(const uint8_t* cmd, size_t len);
    void _parseNotification(const uint8_t* data, size_t len);
    void _parseCellData(const uint8_t* frame, size_t len);
    void _computeCellStats();
    uint8_t _calcChecksum(const uint8_t* data, size_t len);

    NimBLEClient*           _client;
    NimBLERemoteCharacteristic* _charNotify;
    NimBLERemoteCharacteristic* _charWrite;
    NimBLEAdvertisedDevice* _device;

    JkBmsData   _data;
    uint8_t     _rxBuffer[512];
    size_t      _rxLen;
    uint32_t    _lastPollMs;
    bool        _scanning;
    bool        _doConnect;

    JkBmsClientCallbacks _clientCbs;
    JkBmsScanCallbacks   _scanCbs;

    static void _notifyCallback(NimBLERemoteCharacteristic* pChar,
                                uint8_t* pData, size_t length, bool isNotify);
};

extern JkBmsBle jkBms;
