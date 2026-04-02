/*
 * ble_scanner.cpp - Outil de capture BLE pour ESP32
 * Flash sur ESP32, ouvrir le moniteur serie (115200 baud)
 * Affiche TOUS les appareils BLE avec :
 *   - Adresse MAC
 *   - Nom
 *   - RSSI (puissance signal)
 *   - Services UUID
 *   - Manufacturer Data (hex brut)
 *   - Detection automatique JKBMS et Victron
 */
#include <Arduino.h>
#include <NimBLEDevice.h>

// ─── Configuration ───────────────────────────────────────────────────────────
#define SCAN_DURATION_SEC  10    // Duree scan (secondes)
#define SCAN_INTERVAL_SEC  5     // Pause entre scans
#define FILTER_RSSI       -90    // Ignorer si signal trop faible (dBm)

// UUIDs connus
#define JKBMS_SERVICE     "0000ffe0-0000-1000-8000-00805f9b34fb"
#define VICTRON_MANUF_ID  0x02E1

static NimBLEScan* pScan = nullptr;
static int scanCount = 0;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        if (i < len - 1) Serial.print(" ");
    }
}

static const char* identifyDevice(NimBLEAdvertisedDevice* dev) {
    // Detecter JKBMS par UUID service
    if (dev->haveServiceUUID()) {
        if (dev->isAdvertisingService(NimBLEUUID(JKBMS_SERVICE)))
            return "[JKBMS]";
    }
    // Detecter Victron par Manufacturer ID
    if (dev->haveManufacturerData()) {
        std::string md = dev->getManufacturerData();
        if (md.size() >= 2) {
            uint16_t mid = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
            if (mid == VICTRON_MANUF_ID) return "[VICTRON]";
        }
    }
    // Detecter par nom
    if (dev->haveName()) {
        std::string name = dev->getName();
        if (name.find("JK") != std::string::npos) return "[JKBMS?]";
        if (name.find("Victron") != std::string::npos || 
            name.find("SmartSolar") != std::string::npos ||
            name.find("SmartShunt") != std::string::npos ||
            name.find("BMV") != std::string::npos) return "[VICTRON?]";
    }
    return "";
}

// ─── Callback scan ───────────────────────────────────────────────────────────
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        if (dev->getRSSI() < FILTER_RSSI) return;

        const char* tag = identifyDevice(dev);

        Serial.println("────────────────────────────────────────");
        Serial.printf("  MAC      : %s %s\n", dev->getAddress().toString().c_str(), tag);
        Serial.printf("  RSSI     : %d dBm\n", dev->getRSSI());

        if (dev->haveName())
            Serial.printf("  Nom      : %s\n", dev->getName().c_str());

        if (dev->haveServiceUUID()) {
            Serial.printf("  Services : ");
            for (int i = 0; i < dev->getServiceUUIDCount(); i++) {
                Serial.printf("%s ", dev->getServiceUUID(i).toString().c_str());
            }
            Serial.println();
        }

        if (dev->haveManufacturerData()) {
            std::string md = dev->getManufacturerData();
            uint16_t mid = 0;
            if (md.size() >= 2) mid = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
            Serial.printf("  Manuf ID : 0x%04X (%d bytes)\n", mid, md.size());
            Serial.printf("  Data hex : ");
            printHex((const uint8_t*)md.c_str(), md.size());
            Serial.println();

            // Si Victron, decoder le type d'appareil
            if (mid == VICTRON_MANUF_ID && md.size() >= 6) {
                uint8_t recType = (uint8_t)md[2];
                uint16_t modelId = (uint8_t)md[4] | ((uint8_t)md[5] << 8);
                static const char* recTypes[] = {
                    "Solar Charger", "Battery Monitor", "Inverter",
                    "DC/DC", "SmartLithium", "Inverter RS",
                    "GX Device", "AC Charger", "Smart Battery Protect",
                    "Lynx Smart BMS", "Multi RS", "VE.Bus", "DC Energy Meter"
                };
                Serial.printf("  Victron  : Record=%s(0x%02X) Model=0x%04X\n",
                    recType < 13 ? recTypes[recType] : "Unknown", recType, modelId);

                // Afficher les 16 octets de donnees chiffrees
                if (md.size() >= 22) {
                    Serial.printf("  Encrypted: ");
                    printHex((const uint8_t*)md.c_str() + 6, md.size() - 6);
                    Serial.println();
                }
            }
        }

        // Service Data
        if (dev->haveServiceData()) {
            Serial.printf("  SvcData  : ");
            std::string sd = dev->getServiceData();
            printHex((const uint8_t*)sd.c_str(), sd.size());
            Serial.println();
        }

        // TX Power
        if (dev->haveTXPower())
            Serial.printf("  TX Power : %d dBm\n", dev->getTXPower());

        Serial.printf("  Type     : %s\n",
            dev->isConnectable() ? "Connectable" : "Non-connectable (advert only)");
    }
};

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║   ESP32 BLE Scanner / Capture Tool          ║");
    Serial.println("║   Detecte : JKBMS, Victron (MPPT, Shunt)   ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.println();

    NimBLEDevice::init("ESP32-BLE-Scanner");
    pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), false);
    pScan->setActiveScan(true);   // Demande scan response pour plus d'infos
    pScan->setInterval(100);
    pScan->setWindow(99);

    Serial.printf("Config : scan=%ds, pause=%ds, RSSI min=%ddBm\n\n",
                  SCAN_DURATION_SEC, SCAN_INTERVAL_SEC, FILTER_RSSI);
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    scanCount++;
    Serial.println("════════════════════════════════════════════════");
    Serial.printf("  SCAN #%d - %ds ...\n", scanCount, SCAN_DURATION_SEC);
    Serial.println("════════════════════════════════════════════════");

    pScan->start(SCAN_DURATION_SEC, false);

    int found = pScan->getResults().getCount();
    Serial.println();
    Serial.printf(">>> %d appareils trouves (scan #%d)\n\n", found, scanCount);

    pScan->clearResults();
    delay(SCAN_INTERVAL_SEC * 1000);
}
