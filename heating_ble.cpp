/*
 * heating_ble.cpp - Module chauffage BLE GATT
 *
 * Gestion de la connexion GATT au chauffage (c1:02:29:4f:fe:50)
 * Webasto / Espar / thermostat BLE
 *
 * IMPORTANT : Les UUIDs de service/caracteristiques sont provisoires.
 * Identifier avec nRF Connect apres connexion physique au chauffage,
 * puis mettre a jour bluetooth_config.h
 */
#include "heating_ble.h"
#include "../config_base.h"

HeatingBle* HeatingBle::instance = nullptr;
HeatingBle  heatingBle;

// ─── Intervalle de tentative de reconnexion (ms) ─────────────────────────────
#define HEATING_RECONNECT_INTERVAL_MS  10000

// ============================================================
//  Callbacks client GATT
// ============================================================
void HeatingClientCallbacks::onConnect(NimBLEClient* pClient) {
    Serial.println("[Chauffage BLE] Connecte !");
}

void HeatingClientCallbacks::onDisconnect(NimBLEClient* pClient) {
    Serial.println("[Chauffage BLE] Deconnecte.");
    if (HeatingBle::instance) {
        HeatingBle::instance->setConnected(false);
    }
}

// ============================================================
//  Callback notification
// ============================================================
void HeatingNotifyCallbacks::onNotify(NimBLERemoteCharacteristic* pChar,
                                       uint8_t* pData, size_t length, bool isNotify) {
    if (HeatingBle::instance) {
        HeatingBle::instance->onNotifyData(pData, length);
    }
}

// ============================================================
//  Constructeur
// ============================================================
HeatingBle::HeatingBle()
    : _pClient(nullptr), _pService(nullptr),
      _pWriteChar(nullptr), _pNotifyChar(nullptr),
      _lastConnectAttemptMs(0), _connecting(false) {
    memset(&_data, 0, sizeof(_data));
    instance = this;
}

// ============================================================
//  begin()
// ============================================================
void HeatingBle::begin() {
    Serial.printf("[Chauffage BLE] Init — MAC : %s\n", HEATING_BLE_MAC);
    Serial.println("[Chauffage BLE] UUIDs provisoires — identifier avec nRF Connect");
}

// ============================================================
//  update() — gere connexion + reconnexion
// ============================================================
void HeatingBle::update() {
    if (_data.connected) return;  // Deja connecte

    uint32_t now = millis();
    if (now - _lastConnectAttemptMs < HEATING_RECONNECT_INTERVAL_MS) return;
    _lastConnectAttemptMs = now;

    if (!_connecting) {
        _connecting = true;
        Serial.println("[Chauffage BLE] Tentative de connexion...");
        if (_connect()) {
            Serial.println("[Chauffage BLE] Connexion reussie");
        } else {
            Serial.println("[Chauffage BLE] Echec connexion — retry dans 10s");
        }
        _connecting = false;
    }
}

// ============================================================
//  _connect() — connexion GATT
// ============================================================
bool HeatingBle::_connect() {
    NimBLEAddress addr(HEATING_BLE_MAC);

    if (_pClient == nullptr) {
        _pClient = NimBLEDevice::createClient();
        _pClient->setClientCallbacks(&_clientCbs, false);
        _pClient->setConnectionParams(12, 12, 0, 51);
        _pClient->setConnectTimeout(5);
    }

    if (!_pClient->connect(addr)) {
        return false;
    }

    _pService = _pClient->getService(HEATING_SERVICE_UUID);
    if (_pService == nullptr) {
        Serial.printf("[Chauffage BLE] Service %s introuvable\n", HEATING_SERVICE_UUID);
        Serial.println("[Chauffage BLE] Verifier UUID avec nRF Connect");
        _pClient->disconnect();
        return false;
    }

    _pWriteChar = _pService->getCharacteristic(HEATING_CHAR_WRITE_UUID);
    if (_pWriteChar == nullptr) {
        Serial.printf("[Chauffage BLE] Char ecriture %s introuvable\n", HEATING_CHAR_WRITE_UUID);
        _pClient->disconnect();
        return false;
    }

    _pNotifyChar = _pService->getCharacteristic(HEATING_CHAR_NOTIFY_UUID);
    if (_pNotifyChar != nullptr && _pNotifyChar->canNotify()) {
        _pNotifyChar->subscribe(true, HeatingNotifyCallbacks::onNotify, false);
    }

    _data.connected = true;
    return true;
}

// ============================================================
//  _writeCommand()
// ============================================================
bool HeatingBle::_writeCommand(uint8_t* data, size_t len) {
    if (!_data.connected || _pWriteChar == nullptr) {
        Serial.println("[Chauffage BLE] Non connecte — commande ignoree");
        return false;
    }
    bool ok = _pWriteChar->writeValue(data, len, true);
    if (!ok) {
        Serial.println("[Chauffage BLE] Echec ecriture commande");
        _data.connected = false;
    }
    return ok;
}

// ============================================================
//  sendOn() / sendOff()
// ============================================================
bool HeatingBle::sendOn() {
    Serial.println("[Chauffage BLE] -> ON");
    uint8_t cmd[] = { HEATING_CMD_ON };
    _data.is_on = true;
    return _writeCommand(cmd, sizeof(cmd));
}

bool HeatingBle::sendOff() {
    Serial.println("[Chauffage BLE] -> OFF");
    uint8_t cmd[] = { HEATING_CMD_OFF };
    _data.is_on = false;
    return _writeCommand(cmd, sizeof(cmd));
}

bool HeatingBle::sendTemp(float temp) {
    Serial.printf("[Chauffage BLE] -> Consigne %.1f°C\n", temp);
    // Encoder la temperature selon le protocole de l'appareil
    // Format provisoire : [0x02, temp_x10_hi, temp_x10_lo]
    uint16_t t = (uint16_t)(temp * 10);
    uint8_t cmd[] = { 0x02, (uint8_t)(t >> 8), (uint8_t)(t & 0xFF) };
    _data.target_temp = temp;
    return _writeCommand(cmd, sizeof(cmd));
}

// ============================================================
//  onNotifyData() — interpretation des notifications
// ============================================================
void HeatingBle::onNotifyData(uint8_t* pData, size_t length) {
    // Format a adapter selon le protocole de l'appareil
    // Format provisoire suppose :
    //   [0] : etat (0x00=Off, 0x01=On)
    //   [1] : temperature actuelle * 2 (°C x 2)
    //   [2] : consigne * 2
    //   [3] : code erreur
    if (length < 1) return;

    _data.is_on   = (pData[0] != 0x00);
    if (length >= 2) _data.current_temp = pData[1] * 0.5f;
    if (length >= 3) _data.target_temp  = pData[2] * 0.5f;
    if (length >= 4) _data.error_code   = pData[3];

    _data.data_valid     = true;
    _data.last_update_ms = millis();

    Serial.printf("[Chauffage BLE] Etat:%s  Tcur:%.1f°C  Tcible:%.1f°C  Err:%d\n",
                  _data.is_on ? "ON" : "OFF",
                  _data.current_temp, _data.target_temp, _data.error_code);
}
