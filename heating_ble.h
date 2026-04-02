#pragma once
/*
 * heating_ble.h - Module chauffage BLE GATT (Webasto / Espar / thermostat)
 *
 * Appareil : c1:02:29:4f:fe:50
 * Protocole : GATT connecte — UUIDs a identifier avec nRF Connect
 *
 * NOTE : Les UUIDs et commandes sont provisoires.
 * Une fois connecte avec nRF Connect, identifier :
 *   - UUID du service de controle
 *   - UUID de la caracteristique d'ecriture (ON/OFF/temp)
 *   - UUID de la caracteristique de notification (etat, temp)
 * Puis mettre a jour bluetooth_config.h
 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../bluetooth_config.h"

// ============================================================
//  Donnees remontees par le chauffage
// ============================================================
struct HeatingBleData {
    bool     is_on;              // true = chauffage en marche
    float    current_temp;       // °C - Temp actuelle (si disponible)
    float    target_temp;        // °C - Consigne
    uint8_t  power_level;        // % ou W (selon appareil)
    uint8_t  error_code;         // 0 = pas d'erreur
    bool     connected;
    bool     data_valid;
    uint32_t last_update_ms;
};

// ============================================================
//  Callbacks GATT
// ============================================================
class HeatingClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
};

class HeatingNotifyCallbacks {
public:
    static void onNotify(NimBLERemoteCharacteristic* pChar,
                         uint8_t* pData, size_t length, bool isNotify);
};

// ============================================================
//  Classe HeatingBle
// ============================================================
class HeatingBle {
public:
    HeatingBle();
    void begin();
    void update();              // Appeler dans loop() — gere reconnexion

    bool sendOn();              // Allumer le chauffage
    bool sendOff();             // Eteindre le chauffage
    bool sendTemp(float temp);  // Envoyer consigne temperature

    bool isConnected() const { return _data.connected; }
    const HeatingBleData& getData() const { return _data; }
    void setConnected(bool v) { _data.connected = v; }

    static HeatingBle* instance;

    // Appele par le callback notification
    void onNotifyData(uint8_t* pData, size_t length);

private:
    bool _connect();
    void _disconnect();
    bool _writeCommand(uint8_t* data, size_t len);

    NimBLEClient*               _pClient;
    NimBLERemoteService*        _pService;
    NimBLERemoteCharacteristic* _pWriteChar;
    NimBLERemoteCharacteristic* _pNotifyChar;
    HeatingClientCallbacks      _clientCbs;

    HeatingBleData _data;
    uint32_t       _lastConnectAttemptMs;
    bool           _connecting;
};

extern HeatingBle heatingBle;
