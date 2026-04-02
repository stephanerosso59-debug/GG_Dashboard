#pragma once
/*
 * ble_scanner.h — Scanner BLE unifié GG VAN Dashboard
 * ====================================================
 * Orchestre tous les appareils BLE du van en un seul module.
 *
 * Appareils gérés :
 *  ┌─────────────────────────────┬───────────────────────┬────────────────┐
 *  │ Appareil                    │ MAC                   │ Mode BLE       │
 *  ├─────────────────────────────┼───────────────────────┼────────────────┤
 *  │ JK-BMS Lithium              │ C8:47:80:01:CD:31     │ GATT (connect) │
 *  │ Victron SmartSolar MPPT     │ CF:A0:F0:EF:C4:39     │ Advertisement  │
 *  │ Victron SmartBMV 712        │ C5:9F:63:D2:E7:45     │ Advertisement  │
 *  │ Victron BatteryProtect      │ E5:7A:2C:1C:D4:0C     │ Advertisement  │
 *  │ Victron OrionSmart / Shunt  │ DE:5F:86:D7:15:E3     │ Advertisement  │
 *  │ Chauffage BLE               │ C1:02:29:4F:FE:50     │ GATT (connect) │
 *  └─────────────────────────────┴───────────────────────┴────────────────┘
 *
 * Usage dans main.cpp :
 *   #include "ble/ble_scanner.h"
 *   void setup() { bleScanner.begin(); }
 *   void loop()  { bleScanner.update(); }
 *
 * Données accessibles :
 *   bleScanner.victron.getMpptData()
 *   bleScanner.victron.getBmvData()
 *   bleScanner.victron.getOrionData()
 *   bleScanner.victron.getBpData()
 *   bleScanner.jkbms.getData()
 *   bleScanner.getStatus()
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../bluetooth_config.h"
#include "victron_ble.h"
#include "jkbms_ble.h"
#include "heating_ble.h"

// ============================================================
//  État global du scanner
// ============================================================
struct BleScannerStatus {
    // Connectivité
    bool jkbms_seen;          // JKBMS détecté (advertisement)
    bool jkbms_connected;     // JKBMS connecté en GATT
    bool mppt_seen;           // SmartSolar détecté
    bool bmv_seen;            // SmartBMV 712 détecté
    bool bp_seen;             // BatteryProtect détecté
    bool orion_seen;          // OrionSmart/SmartShunt détecté
    bool heating_connected;   // Chauffage connecté

    // Compteurs de mise à jour
    uint32_t mppt_updates;
    uint32_t bmv_updates;
    uint32_t bp_updates;
    uint32_t orion_updates;
    uint32_t jkbms_updates;

    // Timestamps dernière réception (ms)
    uint32_t mppt_last_ms;
    uint32_t bmv_last_ms;
    uint32_t bp_last_ms;
    uint32_t orion_last_ms;
    uint32_t jkbms_last_ms;

    // Scan BLE
    bool     scanning;
    uint32_t scan_count;
    uint32_t last_scan_ms;
};

// ============================================================
//  Callbacks scan BLE unifié
// ============================================================
class BleScannerCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
    void onResult(NimBLEAdvertisedDevice* dev) override;
};

// ============================================================
//  Classe principale BleScanner
// ============================================================
class BleScanner {
public:
    BleScanner();

    // ── Initialisation (appeler dans setup()) ────────────────
    void begin();

    // ── Boucle principale (appeler dans loop()) ──────────────
    void update();

    // ── Accès données ────────────────────────────────────────
    VictronBle  victron;
    JkBms       jkbms;
    HeatingBle  heating;

    const BleScannerStatus& getStatus() const { return _status; }

    // ── Utilitaires ──────────────────────────────────────────
    bool isMacKnown(const std::string& mac) const;
    void printStatus() const;

    // ── Callback interne (utilisé par BleScannerCallbacks) ───
    void onAdvertisement(NimBLEAdvertisedDevice* dev);

    static BleScanner* instance;

private:
    void _startScan();
    void _stopScan();
    bool _macEquals(const std::string& a, const char* b) const;

    BleScannerStatus    _status;
    BleScannerCallbacks _callbacks;
    uint32_t            _lastScanStartMs;

    // Toutes les MAC connues (pour filtrage rapide)
    static const char* _KNOWN_MACS[];
    static const size_t _KNOWN_MAC_COUNT;
};

// Instance globale
extern BleScanner bleScanner;
