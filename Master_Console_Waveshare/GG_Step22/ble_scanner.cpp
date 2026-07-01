/*
 * ble_scanner.cpp — Scanner BLE simple
 */
#include "ble_scanner.h"
#include "bluetooth_config.h"
#include "user_config.h"
#include "victron_ble.h"
#include <NimBLEDevice.h>
#include <ctype.h>
#include <string.h>
#include "esp_heap_caps.h"

// ── État global ───────────────────────────────────────────
BleScannerState bleScanState = {};

// ── Appareils connus (MAC + label) ────────────────────────
struct KnownMac {
    const char* mac;
    const char* label;
};

// Construite a partir de userConfig (peut etre rechargee depuis SD)
static KnownMac KNOWN_DEVICES[6];
static const int N_KNOWN = 6;

static void _init_known_devices() {
    KNOWN_DEVICES[0] = { userConfig.jkbms_mac,         "JKBMS Batterie" };
    KNOWN_DEVICES[1] = { userConfig.victron_mppt_mac,  "Victron MPPT 100/30" };
    KNOWN_DEVICES[2] = { userConfig.victron_bmv_mac,   "Victron BMV 712" };
    KNOWN_DEVICES[3] = { userConfig.victron_bp_mac,    "Victron BatteryProtect" };
    KNOWN_DEVICES[4] = { userConfig.victron_orion_mac, "Victron Orion/SmartShunt" };
    KNOWN_DEVICES[5] = { userConfig.heating_mac,       "Chauffage Nordkapp" };
}

// Comparaison MAC insensible à la casse
static bool mac_equal(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? (*a - 32) : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? (*b - 32) : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static const char* lookup_known(const char* mac) {
    for (int i = 0; i < N_KNOWN; i++) {
        if (mac_equal(mac, KNOWN_DEVICES[i].mac)) {
            return KNOWN_DEVICES[i].label;
        }
    }
    return nullptr;
}

// ── Ajouter ou mettre à jour un appareil ──────────────────
static void add_or_update_device(const std::string& mac_str, const std::string& name_str, int rssi) {
    // Chercher si déjà présent
    for (int i = 0; i < bleScanState.count; i++) {
        if (strcasecmp(bleScanState.devices[i].mac, mac_str.c_str()) == 0) {
            bleScanState.devices[i].rssi = rssi;
            bleScanState.devices[i].last_seen_ms = millis();
            if (bleScanState.devices[i].name[0] == 0 && !name_str.empty()) {
                strncpy(bleScanState.devices[i].name, name_str.c_str(), NAME_MAX_LEN - 1);
                bleScanState.devices[i].name[NAME_MAX_LEN - 1] = 0;
            }
            return;
        }
    }
    
    // Ajouter (ou remplacer le plus ancien si plein)
    int slot_idx;
    if (bleScanState.count >= MAX_BLE_DEVICES) {
        // Trouver le plus ancien non-connu
        slot_idx = -1;
        uint32_t oldest_ms = UINT32_MAX;
        for (int i = 0; i < bleScanState.count; i++) {
            if (bleScanState.devices[i].is_known) continue;
            if (bleScanState.devices[i].last_seen_ms < oldest_ms) {
                oldest_ms = bleScanState.devices[i].last_seen_ms;
                slot_idx = i;
            }
        }
        if (slot_idx < 0) return;  // tous connus : on ignore
    } else {
        slot_idx = bleScanState.count;
        bleScanState.count++;
    }
    
    BleDeviceInfo& d = bleScanState.devices[slot_idx];
    strncpy(d.mac, mac_str.c_str(), MAC_STR_LEN - 1);
    d.mac[MAC_STR_LEN - 1] = 0;
    strncpy(d.name, name_str.c_str(), NAME_MAX_LEN - 1);
    d.name[NAME_MAX_LEN - 1] = 0;
    d.rssi = rssi;
    d.last_seen_ms = millis();
    d.known_label = lookup_known(d.mac);
    d.is_known = (d.known_label != nullptr);
}

// ── Callback scan NimBLE ──────────────────────────────────
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        std::string mac  = dev->getAddress().toString();
        std::string name = dev->haveName() ? dev->getName() : std::string("");
        int rssi = dev->getRSSI();
        add_or_update_device(mac, name, rssi);
        
        // Si manufacturer data de Victron (0x02E1) : passer au parser
        if (dev->haveManufacturerData()) {
            std::string mfr = dev->getManufacturerData();
            if (mfr.size() >= 3) {
                uint16_t company_id = (uint8_t)mfr[0] | ((uint8_t)mfr[1] << 8);
                if (company_id == 0x02E1) {  // Victron Energy
                    const uint8_t* payload = (const uint8_t*)mfr.data() + 2;
                    size_t plen = mfr.size() - 2;
                    victron_on_advertisement(mac.c_str(), payload, plen);
                }
            }
        }
    }
};
static ScanCallbacks scan_callbacks;

// ============================================================
//  ble_scanner_init
// ============================================================
void ble_scanner_init() {
    Serial0.println("[BLE Scan] Init NimBLE...");
    
    // Charger les MAC connues depuis userConfig (qui a ete charge depuis SD si dispo)
    _init_known_devices();

    // Diag RAM interne avant init BT (le controleur BT a besoin d'un gros bloc interne contigu)
    Serial0.printf("[BLE Scan] RAM interne avant init BT : %u o libres, plus gros bloc = %u o\n",
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    NimBLEDevice::init("GG_VAN");
    NimBLEDevice::setPower(9);  // +9 dBm (NimBLE 2.x : dBm au lieu de esp_power_level_t)
    
    bleScanState.count = 0;
    bleScanState.total_scans = 0;
    bleScanState.last_scan_ms = 0;
    bleScanState.initialized = true;
    
    // Pré-remplir avec nos appareils connus (rssi=0 = jamais vu)
    for (int i = 0; i < N_KNOWN && i < MAX_BLE_DEVICES; i++) {
        BleDeviceInfo& d = bleScanState.devices[i];
        strncpy(d.mac, KNOWN_DEVICES[i].mac, MAC_STR_LEN - 1);
        d.mac[MAC_STR_LEN - 1] = 0;
        d.name[0] = 0;
        d.rssi = 0;
        d.last_seen_ms = 0;
        d.is_known = true;
        d.known_label = KNOWN_DEVICES[i].label;
        bleScanState.count++;
    }
    
    Serial0.printf("[BLE Scan] %d appareils connus pre-charges\n", bleScanState.count);
}

// ============================================================
//  ble_scanner_update — Lance un scan périodique
// ============================================================
void ble_scanner_update() {
    if (!bleScanState.initialized) return;
    
    uint32_t now = millis();
    if (now - bleScanState.last_scan_ms < BLE_SCAN_INTERVAL_MS) return;
    
    // Ne pas lancer de nouveau scan si un scan est deja en cours
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan->isScanning()) return;
    
    bleScanState.last_scan_ms = now;
    
    scan->setScanCallbacks(&scan_callbacks, false);
    scan->setActiveScan(false);        // passif : basse conso
    scan->setInterval(160);
    scan->setWindow(80);
    scan->start(BLE_SCAN_DURATION_S * 1000, false);  // non-bloquant (NimBLE 2.x : duree en MS)
    
    bleScanState.total_scans++;
    
    // Marquer comme "perdu" (rssi=0) les appareils connus non vus récemment
    for (int i = 0; i < bleScanState.count; i++) {
        if (bleScanState.devices[i].is_known &&
            bleScanState.devices[i].last_seen_ms != 0 &&
            (now - bleScanState.devices[i].last_seen_ms) > BLE_DEVICE_TIMEOUT_MS) {
            bleScanState.devices[i].rssi = 0;
            bleScanState.devices[i].last_seen_ms = 0;
        }
    }
    
    Serial0.printf("[BLE Scan] #%lu demarre (%ds) - %d devices total\n",
        bleScanState.total_scans, BLE_SCAN_DURATION_S, bleScanState.count);
}
