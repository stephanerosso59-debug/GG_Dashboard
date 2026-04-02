/*
 * ble_scanner.cpp — Scanner BLE unifié GG VAN Dashboard
 * =====================================================
 * Dépendances : NimBLE-Arduino, mbedtls (AES)
 *
 * Principe de fonctionnement :
 *   1. Scan passif BLE toutes les VICTRON_SCAN_INTERVAL_MS ms
 *      → Filtre sur manufacturer_id Victron (0x02E1)
 *      → Dispatch vers VictronBle::onAdvertisement()
 *   2. Détection JKBMS par MAC → connexion GATT active
 *      → Poll toutes les JKBMS_POLL_INTERVAL_MS ms
 *   3. Connexion GATT chauffage → commandes ON/OFF/setpoint
 */

#include "ble_scanner.h"

// ── Instance globale ─────────────────────────────────────────
BleScanner* BleScanner::instance = nullptr;
BleScanner  bleScanner;

// ── Liste des MAC connues (minuscules, séparées par ':') ─────
const char* BleScanner::_KNOWN_MACS[] = {
    JKBMS_MAC_ADDRESS,
    VICTRON_MPPT_MAC,
    VICTRON_BMV_MAC,
    VICTRON_BP_MAC,
    VICTRON_ORION_MAC,      // = VICTRON_SMARTSHUNT_MAC
    HEATING_BLE_MAC
};
const size_t BleScanner::_KNOWN_MAC_COUNT =
    sizeof(_KNOWN_MACS) / sizeof(_KNOWN_MACS[0]);

// ============================================================
//  BleScannerCallbacks::onResult
//  Appelé par NimBLE pour chaque advertisement reçu
// ============================================================
void BleScannerCallbacks::onResult(NimBLEAdvertisedDevice* dev) {
    if (BleScanner::instance)
        BleScanner::instance->onAdvertisement(dev);
}

// ============================================================
//  Constructeur
// ============================================================
BleScanner::BleScanner() {
    instance = this;
    memset(&_status, 0, sizeof(_status));
    _lastScanStartMs = 0;
}

// ============================================================
//  begin() — Initialisation NimBLE + modules
// ============================================================
void BleScanner::begin() {
    Serial.println("[BLE] Initialisation scanner...");

    NimBLEDevice::init("GG_VAN_DASHBOARD");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // Puissance max

    // Initialise les modules
    victron.begin();   // enregistre ses propres callbacks via victronBle
    jkbms.begin();
    heating.begin();

    // Lance le premier scan
    _startScan();

    Serial.println("[BLE] Scanner démarré");
    Serial.printf("[BLE] %zu appareils connus :\n", _KNOWN_MAC_COUNT);
    for (size_t i = 0; i < _KNOWN_MAC_COUNT; i++)
        Serial.printf("      [%zu] %s\n", i, _KNOWN_MACS[i]);
}

// ============================================================
//  update() — À appeler dans loop()
// ============================================================
void BleScanner::update() {
    uint32_t now = millis();

    // ── Relance scan périodique ──────────────────────────────
    if (!_status.scanning &&
        (now - _lastScanStartMs) >= VICTRON_SCAN_INTERVAL_MS) {
        _startScan();
    }

    // ── Mise à jour JKBMS (poll GATT) ───────────────────────
    jkbms.update();

    // ── Mise à jour chauffage BLE ────────────────────────────
    heating.update();

    // ── Mise à jour statuts connexion ───────────────────────
    _status.jkbms_connected   = jkbms.isConnected();
    _status.heating_connected = heating.isConnected();

    // ── Détection timeouts (appareil disparu > 30 s) ────────
    const uint32_t TIMEOUT_MS = 30000;
    if (_status.mppt_seen && (now - _status.mppt_last_ms) > TIMEOUT_MS)
        _status.mppt_seen = false;
    if (_status.bmv_seen  && (now - _status.bmv_last_ms)  > TIMEOUT_MS)
        _status.bmv_seen  = false;
    if (_status.bp_seen   && (now - _status.bp_last_ms)   > TIMEOUT_MS)
        _status.bp_seen   = false;
    if (_status.orion_seen && (now - _status.orion_last_ms) > TIMEOUT_MS)
        _status.orion_seen = false;
}

// ============================================================
//  onAdvertisement() — Dispatch par MAC
// ============================================================
void BleScanner::onAdvertisement(NimBLEAdvertisedDevice* dev) {
    std::string mac = dev->getAddress().toString();
    uint32_t    now = millis();

    // ── JKBMS ────────────────────────────────────────────────
    if (_macEquals(mac, JKBMS_MAC_ADDRESS)) {
        _status.jkbms_seen    = true;
        _status.jkbms_last_ms = now;
        // La connexion GATT est gérée par jkbms.update()
        return;
    }

    // ── Appareils Victron (advertisement passif + AES decrypt) ─
    // Vérification manufacturer ID Victron (0x02E1)
    if (dev->haveManufacturerData()) {
        std::string mfr = dev->getManufacturerData();
        if (mfr.size() >= 2) {
            uint16_t mfr_id = (uint8_t)mfr[0] | ((uint8_t)mfr[1] << 8);
            if (mfr_id == VICTRON_MANUFACTURER_ID) {

                if (_macEquals(mac, VICTRON_MPPT_MAC)) {
                    _status.mppt_seen    = true;
                    _status.mppt_last_ms = now;
                    _status.mppt_updates++;
                }
                else if (_macEquals(mac, VICTRON_BMV_MAC)) {
                    _status.bmv_seen    = true;
                    _status.bmv_last_ms = now;
                    _status.bmv_updates++;
                }
                else if (_macEquals(mac, VICTRON_BP_MAC)) {
                    _status.bp_seen    = true;
                    _status.bp_last_ms = now;
                    _status.bp_updates++;
                }
                else if (_macEquals(mac, VICTRON_ORION_MAC)) {
                    // OrionSmart ET SmartShunt partagent la même MAC
                    // Le dispatch par record_type est géré dans victron_ble.cpp
                    _status.orion_seen    = true;
                    _status.orion_last_ms = now;
                    _status.orion_updates++;
                }

                // Délégation au parser Victron (AES decrypt + struct fill)
                victron.onAdvertisement(dev);
            }
        }
    }

    // ── Chauffage (filtrage MAC seulement — connexion GATT async) ─
    if (_macEquals(mac, HEATING_BLE_MAC)) {
        // heating.update() gère la reconnexion si nécessaire
        return;
    }
}

// ============================================================
//  _startScan()
// ============================================================
void BleScanner::_startScan() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&_callbacks, true);
    scan->setActiveScan(false);   // Passif = économie énergie pour Victron
    scan->setInterval(160);       // 100 ms
    scan->setWindow(80);          //  50 ms

    // Durée du scan : 5 s non-bloquant
    scan->start(5, false);

    _status.scanning     = true;
    _status.scan_count++;
    _lastScanStartMs     = millis();

    // Arrêt automatique après 5 s (NimBLE callback interne)
    // On considère le scan terminé après VICTRON_SCAN_INTERVAL_MS
}

// ============================================================
//  _stopScan()
// ============================================================
void BleScanner::_stopScan() {
    NimBLEDevice::getScan()->stop();
    _status.scanning = false;
}

// ============================================================
//  isMacKnown()
// ============================================================
bool BleScanner::isMacKnown(const std::string& mac) const {
    for (size_t i = 0; i < _KNOWN_MAC_COUNT; i++)
        if (_macEquals(mac, _KNOWN_MACS[i])) return true;
    return false;
}

// ============================================================
//  _macEquals() — Comparaison insensible à la casse
// ============================================================
bool BleScanner::_macEquals(const std::string& a, const char* b) const {
    std::string sb(b);
    if (a.size() != sb.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (toupper(a[i]) != toupper(sb[i])) return false;
    return true;
}

// ============================================================
//  printStatus() — Debug Serial
// ============================================================
void BleScanner::printStatus() const {
    Serial.println("===== BLE Scanner Status =====");
    Serial.printf("Scans: %lu  |  Scanning: %s\n",
        _status.scan_count, _status.scanning ? "OUI" : "NON");
    Serial.println("--- Victron ---");
    Serial.printf("  SmartSolar MPPT  %s  (%s)  mises à jour: %lu\n",
        VICTRON_MPPT_MAC,
        _status.mppt_seen ? "VU " : "---",
        _status.mppt_updates);
    Serial.printf("  SmartBMV 712     %s  (%s)  mises à jour: %lu\n",
        VICTRON_BMV_MAC,
        _status.bmv_seen ? "VU " : "---",
        _status.bmv_updates);
    Serial.printf("  BatteryProtect   %s  (%s)  mises à jour: %lu\n",
        VICTRON_BP_MAC,
        _status.bp_seen ? "VU " : "---",
        _status.bp_updates);
    Serial.printf("  OrionSmart/Shunt %s  (%s)  mises à jour: %lu\n",
        VICTRON_ORION_MAC,
        _status.orion_seen ? "VU " : "---",
        _status.orion_updates);
    Serial.println("--- JK-BMS ---");
    Serial.printf("  %s  Vu:%s  Connecté:%s  màj:%lu\n",
        JKBMS_MAC_ADDRESS,
        _status.jkbms_seen      ? "OUI" : "NON",
        _status.jkbms_connected ? "OUI" : "NON",
        _status.jkbms_updates);
    Serial.println("--- Chauffage ---");
    Serial.printf("  %s  Connecté:%s\n",
        HEATING_BLE_MAC,
        _status.heating_connected ? "OUI" : "NON");
    Serial.println("==============================");
}
