/*
 * jkbms_ble.cpp - Implementation BLE JKBMS (NimBLE)
 */
#include "jkbms_ble.h"
#include "../config_base.h"
#include <Arduino.h>

JkBmsBle* JkBmsBle::instance = nullptr;
JkBmsBle  jkBms;

// ============================================================
//  Callbacks client
// ============================================================
void JkBmsClientCallbacks::onConnect(NimBLEClient* pClient) {
    Serial.println("[JKBMS] Connecte");
}

void JkBmsClientCallbacks::onDisconnect(NimBLEClient* pClient) {
    Serial.println("[JKBMS] Deconnecte");
    if (JkBmsBle::instance) {
        JkBmsBle::instance->_data.connected = false;
        JkBmsBle::instance->_data.data_valid = false;
        JkBmsBle::instance->_doConnect = false;
        JkBmsBle::instance->_scanning = false;
    }
}

// ============================================================
//  Callbacks scan
// ============================================================
void JkBmsScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (!JkBmsBle::instance) return;
    String mac = advertisedDevice->getAddress().toString().c_str();
    mac.toUpperCase();
    String target = String(JKBMS_MAC_ADDRESS);
    target.toUpperCase();
    if (mac == target) {
        Serial.printf("[JKBMS] Appareil trouve : %s\n", mac.c_str());
        NimBLEDevice::getScan()->stop();
        JkBmsBle::instance->_device = advertisedDevice;
        JkBmsBle::instance->_doConnect = true;
    }
}

// ============================================================
//  Callback notification (static)
// ============================================================
void JkBmsBle::_notifyCallback(NimBLERemoteCharacteristic* pChar,
                                uint8_t* pData, size_t length, bool isNotify) {
    if (!JkBmsBle::instance) return;
    JkBmsBle::instance->_parseNotification(pData, length);
}

// ============================================================
//  Constructeur
// ============================================================
JkBmsBle::JkBmsBle()
    : _client(nullptr), _charNotify(nullptr), _charWrite(nullptr),
      _device(nullptr), _rxLen(0), _lastPollMs(0),
      _scanning(false), _doConnect(false) {
    memset(&_data, 0, sizeof(_data));
    instance = this;
}

// ============================================================
//  begin()
// ============================================================
void JkBmsBle::begin() {
    Serial.println("[JKBMS] Initialisation BLE...");
    _startScan();
}

// ============================================================
//  update() - appeler dans loop()
// ============================================================
void JkBmsBle::update() {
    if (_doConnect && _device) {
        _doConnect = false;
        _connect(_device);
    }

    if (!_data.connected && !_scanning) {
        // Relancer le scan periodiquement
        if (millis() - _lastPollMs > 10000) {
            _lastPollMs = millis();
            _startScan();
        }
        return;
    }

    if (_data.connected) {
        if (millis() - _lastPollMs >= JKBMS_POLL_INTERVAL_MS) {
            _lastPollMs = millis();
            _sendCommand(JKBMS_CMD_CELL_DATA, sizeof(JKBMS_CMD_CELL_DATA));
        }
    }
}

// ============================================================
//  _startScan()
// ============================================================
void JkBmsBle::_startScan() {
    _scanning = true;
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&_scanCbs, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->start(10, false);  // 10 secondes
    Serial.println("[JKBMS] Scan BLE demarre...");
}

// ============================================================
//  _connect()
// ============================================================
void JkBmsBle::_connect(NimBLEAdvertisedDevice* device) {
    _scanning = false;
    if (_client == nullptr) {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(&_clientCbs, false);
        _client->setConnectionParams(12, 12, 0, 51);
        _client->setConnectTimeout(5);
    }

    if (!_client->connect(device)) {
        Serial.println("[JKBMS] Echec connexion");
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return;
    }

    NimBLERemoteService* pService = _client->getService(JKBMS_SERVICE_UUID);
    if (!pService) {
        Serial.println("[JKBMS] Service introuvable");
        _client->disconnect();
        return;
    }

    _charNotify = pService->getCharacteristic(JKBMS_CHAR_NOTIFY_UUID);
    _charWrite  = pService->getCharacteristic(JKBMS_CHAR_WRITE_UUID);

    if (!_charNotify || !_charWrite) {
        Serial.println("[JKBMS] Caracteristiques introuvables");
        _client->disconnect();
        return;
    }

    if (_charNotify->canNotify()) {
        _charNotify->subscribe(true, _notifyCallback);
    }

    _data.connected = true;
    _rxLen = 0;
    Serial.println("[JKBMS] Connexion reussie");

    // Premier poll immediat
    delay(500);
    _sendCommand(JKBMS_CMD_CELL_DATA, sizeof(JKBMS_CMD_CELL_DATA));
}

// ============================================================
//  _sendCommand()
// ============================================================
void JkBmsBle::_sendCommand(const uint8_t* cmd, size_t len) {
    if (!_client || !_client->isConnected() || !_charWrite) return;
    _rxLen = 0;
    _charWrite->writeValue(cmd, len, false);
}

// ============================================================
//  _parseNotification()  - Accumule les paquets
// ============================================================
void JkBmsBle::_parseNotification(const uint8_t* data, size_t len) {
    if (_rxLen + len > sizeof(_rxBuffer)) {
        _rxLen = 0;
    }
    memcpy(_rxBuffer + _rxLen, data, len);
    _rxLen += len;

    // Detecter la fin de trame : header 0x55 0xAA 0xEB 0x90
    if (_rxLen < 4) return;
    if (_rxBuffer[0] == 0x55 && _rxBuffer[1] == 0xAA &&
        _rxBuffer[2] == 0xEB && _rxBuffer[3] == 0x90) {
        if (_rxLen >= 6) {
            uint16_t frameLen = (_rxBuffer[4]) | ((uint16_t)_rxBuffer[5] << 8);
            if (_rxLen >= (size_t)(frameLen + 6)) {
                _parseCellData(_rxBuffer, _rxLen);
                _rxLen = 0;
            }
        }
    } else {
        // Rejet si tete invalide
        _rxLen = 0;
    }
}

// ============================================================
//  _parseCellData() - Decodage trame cellules JK-BMS
//  Format base sur syssi/esphome-jk-bms
// ============================================================
void JkBmsBle::_parseCellData(const uint8_t* frame, size_t len) {
    if (len < 300) return;  // Trame cell data minimale ~300 octets

    const uint8_t* d = frame + 6;  // Sauter le header de 6 octets

    // --- Tensions cellules ---
    // Offset 0x00 : nombre de cellules (1 octet)
    uint8_t cell_count = d[0];
    if (cell_count == 0 || cell_count > JKBMS_MAX_CELLS) return;

    _data.cells.cell_count = cell_count;
    for (uint8_t i = 0; i < cell_count; i++) {
        uint16_t raw = (uint16_t)(d[1 + i * 2]) | ((uint16_t)(d[2 + i * 2]) << 8);
        _data.cells.cell_voltage[i] = raw * 0.001f;  // mV -> V
    }

    // Offset apres tensions : 1 + cell_count*2
    size_t offset = 1 + cell_count * 2;

    // Resistances cellules (cell_count * 2 octets) - on passe
    offset += cell_count * 2;

    // Tension totale batterie (4 octets, uint32, *0.001 = V)
    if (offset + 4 > len) return;
    uint32_t raw_voltage = (uint32_t)d[offset]        |
                           ((uint32_t)d[offset+1] << 8)  |
                           ((uint32_t)d[offset+2] << 16) |
                           ((uint32_t)d[offset+3] << 24);
    _data.battery_voltage = raw_voltage * 0.001f;
    offset += 4;

    // Courant (4 octets, int32, *0.001 = A)
    if (offset + 4 > len) return;
    int32_t raw_current = (int32_t)d[offset]        |
                          ((int32_t)d[offset+1] << 8)  |
                          ((int32_t)d[offset+2] << 16) |
                          ((int32_t)d[offset+3] << 24);
    _data.battery_current = raw_current * 0.001f;
    offset += 4;

    // Skip temperature MOS (2 octets)
    if (offset + 2 > len) return;
    _data.temp_mos = ((uint16_t)d[offset] | ((uint16_t)d[offset+1] << 8)) * 0.1f;
    offset += 2;

    // Temp sonde 1
    if (offset + 2 > len) return;
    _data.temp_sensor1 = ((uint16_t)d[offset] | ((uint16_t)d[offset+1] << 8)) * 0.1f;
    offset += 2;

    // Temp sonde 2
    if (offset + 2 > len) return;
    _data.temp_sensor2 = ((uint16_t)d[offset] | ((uint16_t)d[offset+1] << 8)) * 0.1f;
    offset += 2;

    // SOC (2 octets, uint16, %)
    if (offset + 2 > len) return;
    _data.battery_soc = (float)((uint16_t)d[offset] | ((uint16_t)d[offset+1] << 8));
    offset += 2;

    // Capacite restante Ah (4 octets, uint32, *0.001)
    if (offset + 4 > len) return;
    uint32_t raw_cap = (uint32_t)d[offset]        |
                       ((uint32_t)d[offset+1] << 8)  |
                       ((uint32_t)d[offset+2] << 16) |
                       ((uint32_t)d[offset+3] << 24);
    _data.battery_capacity_ah = raw_cap * 0.001f;
    offset += 4;

    // Nombre de cycles (4 octets)
    if (offset + 4 > len) return;
    _data.cycle_count = (uint32_t)d[offset]        |
                        ((uint32_t)d[offset+1] << 8)  |
                        ((uint32_t)d[offset+2] << 16) |
                        ((uint32_t)d[offset+3] << 24);
    offset += 4;

    // Bits de statut (2 octets)
    if (offset + 2 > len) return;
    uint16_t status = (uint16_t)d[offset] | ((uint16_t)d[offset+1] << 8);
    _data.charging_enabled     = (status >> 0) & 1;
    _data.discharging_enabled  = (status >> 1) & 1;
    _data.balancing_active     = (status >> 2) & 1;
    _data.charging_fet_on      = (status >> 3) & 1;
    _data.discharging_fet_on   = (status >> 4) & 1;
    _data.overvoltage_protection   = (status >> 8)  & 1;
    _data.undervoltage_protection  = (status >> 9)  & 1;
    _data.overcurrent_protection   = (status >> 10) & 1;
    _data.overtemp_protection      = (status >> 11) & 1;
    _data.short_circuit_protection = (status >> 12) & 1;

    _data.battery_power = _data.battery_voltage * _data.battery_current;

    _computeCellStats();

    _data.data_valid    = true;
    _data.last_update_ms = millis();
}

// ============================================================
//  _computeCellStats()
// ============================================================
void JkBmsBle::_computeCellStats() {
    if (_data.cells.cell_count == 0) return;
    float vmin = _data.cells.cell_voltage[0];
    float vmax = _data.cells.cell_voltage[0];
    float vsum = 0.0f;
    uint8_t imin = 0, imax = 0;

    for (uint8_t i = 0; i < _data.cells.cell_count; i++) {
        float v = _data.cells.cell_voltage[i];
        vsum += v;
        if (v < vmin) { vmin = v; imin = i; }
        if (v > vmax) { vmax = v; imax = i; }
    }
    _data.cells.cell_voltage_min = vmin;
    _data.cells.cell_voltage_max = vmax;
    _data.cells.cell_voltage_avg = vsum / _data.cells.cell_count;
    _data.cells.cell_delta       = (vmax - vmin) * 1000.0f;  // en mV
    _data.cells.cell_min_index   = imin;
    _data.cells.cell_max_index   = imax;
}

// ============================================================
//  _calcChecksum()
// ============================================================
uint8_t JkBmsBle::_calcChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return sum;
}
