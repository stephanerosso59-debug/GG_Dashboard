/*
 * victron_ble.cpp - Implementation Victron BLE Advertisement
 * Protocol Victron "Instant readout" AES-128-CTR
 * Reference : Fabian-Schmidt/esphome-victron_ble
 *
 * Appareils :
 *   SmartSolar MPPT (CF:A0:F0:EF:C4:39)
 *   SmartBMV 712    (C5:9F:63:D2:E7:45)
 *   BatteryProtect  (E5:7A:2C:1C:D4:0C)
 *   OrionSmart      (DE:5F:86:D7:15:E3)
 */
#include "victron_ble.h"
#include "../config_base.h"
#include <mbedtls/aes.h>

VictronBle* VictronBle::instance = nullptr;
VictronBle  victronBle;

// ============================================================
//  Scan callback
// ============================================================
void VictronScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (VictronBle::instance) {
        VictronBle::instance->onAdvertisement(advertisedDevice);
    }
}

// ============================================================
//  Constructeur
// ============================================================
VictronBle::VictronBle() : _lastScanMs(0) {
    memset(&_mppt,       0, sizeof(_mppt));
    memset(&_smartshunt, 0, sizeof(_smartshunt));
    memset(&_bmv,        0, sizeof(_bmv));
    memset(&_bp,         0, sizeof(_bp));
    memset(&_orion,      0, sizeof(_orion));
    _bmv.time_to_go        = -1.0f;
    _smartshunt.time_to_go = -1.0f;
    instance = this;
}

// ============================================================
//  begin()
// ============================================================
void VictronBle::begin() {
    Serial.println("[Victron] Demarrage scan BLE (4 appareils)...");
    Serial.printf("[Victron]  MPPT       : %s\n", VICTRON_MPPT_MAC);
    Serial.printf("[Victron]  SmartBMV   : %s\n", VICTRON_BMV_MAC);
    Serial.printf("[Victron]  BattProt   : %s\n", VICTRON_BP_MAC);
    Serial.printf("[Victron]  SmartShunt + Orion : %s\n", VICTRON_ORION_MAC);
}

// ============================================================
//  update() - Relance le scan periodiquement
// ============================================================
void VictronBle::update() {
    if (millis() - _lastScanMs >= VICTRON_SCAN_INTERVAL_MS) {
        _lastScanMs = millis();
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if (!pScan->isScanning()) {
            pScan->setAdvertisedDeviceCallbacks(&_scanCbs, false);
            pScan->setActiveScan(false);
            pScan->setInterval(100);
            pScan->setWindow(99);
            pScan->start(2, false);
        }
    }
}

// ============================================================
//  onAdvertisement() — filtrage par MAC
// ============================================================
void VictronBle::onAdvertisement(NimBLEAdvertisedDevice* device) {
    if (!device->haveManufacturerData()) return;

    std::string manuData_std = device->getManufacturerData();
    if (manuData_std.size() < 4) return;

    const uint8_t* manuData = (const uint8_t*)manuData_std.c_str();

    // Verifier le Manufacturer ID Victron (0x02E1, little-endian)
    uint16_t manuId = (uint16_t)manuData[0] | ((uint16_t)manuData[1] << 8);
    if (manuId != VICTRON_MANUFACTURER_ID) return;

    String mac = device->getAddress().toString().c_str();
    mac.toUpperCase();

    // SmartSolar MPPT
#if VICTRON_MPPT_ENABLED
    {
        String ref = String(VICTRON_MPPT_MAC); ref.toUpperCase();
        if (mac == ref) {
            uint8_t key[16]; _hexToBytes(VICTRON_MPPT_BINDKEY, key, 16);
            _parseAdvertisement(device, key, DEV_MPPT);
            _mppt.connected = true;
            return;
        }
    }
#endif

    // SmartBMV 712
#if VICTRON_BMV_ENABLED
    {
        String ref = String(VICTRON_BMV_MAC); ref.toUpperCase();
        if (mac == ref) {
            uint8_t key[16]; _hexToBytes(VICTRON_BMV_BINDKEY, key, 16);
            _parseAdvertisement(device, key, DEV_BMV);
            _bmv.connected = true;
            return;
        }
    }
#endif

    // BatteryProtect
#if VICTRON_BP_ENABLED
    {
        String ref = String(VICTRON_BP_MAC); ref.toUpperCase();
        if (mac == ref) {
            uint8_t key[16]; _hexToBytes(VICTRON_BP_BINDKEY, key, 16);
            _parseAdvertisement(device, key, DEV_BP);
            _bp.connected = true;
            return;
        }
    }
#endif

    // SmartShunt + OrionSmart : meme MAC DE:5F:86:D7:15:E3
    // Le readout_type dans le paquet determine de quel appareil il s'agit
#if VICTRON_ORION_ENABLED
    {
        String ref = String(VICTRON_ORION_MAC); ref.toUpperCase();
        if (mac == ref) {
            uint8_t key[16]; _hexToBytes(VICTRON_ORION_BINDKEY, key, 16);
            // Lire le readout_type avant de dispatcher
            std::string manuStr = device->getManufacturerData();
            if (manuStr.size() >= (2 + sizeof(VictronBleRecordBase))) {
                const VictronBleRecordBase* base =
                    (const VictronBleRecordBase*)(manuStr.c_str() + 2);
                if (base->readout_type == VICTRON_RECORD_DCDC_CONVERTER) {
                    _parseAdvertisement(device, key, DEV_ORION);
                    _orion.connected = true;
                } else {
                    // Par defaut : SmartShunt (battery monitor)
                    _parseAdvertisement(device, key, DEV_SMARTSHUNT);
                    _smartshunt.connected = true;
                }
            }
            return;
        }
    }
#endif
}

// ============================================================
//  _parseAdvertisement()
// ============================================================
void VictronBle::_parseAdvertisement(NimBLEAdvertisedDevice* device,
                                      const uint8_t* bindkey, DeviceType type) {
    std::string manuData_std = device->getManufacturerData();
    const uint8_t* d = (const uint8_t*)manuData_std.c_str();
    size_t len = manuData_std.size();

    if (len < (2 + sizeof(VictronBleRecordBase) + 4)) return;

    const VictronBleRecordBase* base = (const VictronBleRecordBase*)(d + 2);

    if (base->encryption_key_0 != bindkey[0]) {
        Serial.printf("[Victron] Bindkey invalide pour type %d !\n", (int)type);
        return;
    }

    const uint8_t* encData = d + 2 + sizeof(VictronBleRecordBase);
    size_t encLen = len - 2 - sizeof(VictronBleRecordBase);
    if (encLen > VICTRON_ENCRYPTED_DATA_MAX_SIZE) encLen = VICTRON_ENCRYPTED_DATA_MAX_SIZE;

    uint8_t macBytes[6];
    NimBLEAddress addr = device->getAddress();
    memcpy(macBytes, addr.getNative(), 6);

    uint8_t decrypted[VICTRON_ENCRYPTED_DATA_MAX_SIZE] = {0};
    if (!_decrypt(encData, encLen, bindkey, macBytes, base->record_id, decrypted, encLen)) {
        Serial.println("[Victron] Dechiffrement echoue");
        return;
    }

    switch (type) {
        case DEV_MPPT:       _parseSolarCharger(decrypted);   break;
        case DEV_SMARTSHUNT: _parseSmartshunt(decrypted);     break;
        case DEV_BMV:        _parseBatteryMonitor(decrypted); break;
        case DEV_BP:         _parseBatteryProtect(decrypted); break;
        case DEV_ORION:      _parseOrionSmart(decrypted);     break;
    }
}

// ============================================================
//  _decrypt() - AES-128-CTR
// ============================================================
bool VictronBle::_decrypt(const uint8_t* payload, size_t payloadLen,
                           const uint8_t* bindkey,
                           const uint8_t* mac,
                           uint16_t record_id,
                           uint8_t* outData, size_t outLen) {
    uint8_t nonce[16] = {0};
    for (int i = 0; i < 6; i++) nonce[i] = mac[5 - i];
    nonce[6] = (uint8_t)(record_id & 0xFF);
    nonce[7] = (uint8_t)(record_id >> 8);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, bindkey, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    uint8_t stream_block[16] = {0};
    size_t nc_off = 0;
    uint8_t counter[16];
    memcpy(counter, nonce, 16);

    int ret = mbedtls_aes_crypt_ctr(&aes,
                                     (payloadLen < outLen) ? payloadLen : outLen,
                                     &nc_off, counter, stream_block,
                                     payload, outData);
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

// ============================================================
//  _parseSmartshunt() - SmartShunt (courant/SOC/energie)
//  Format advertisement SmartShunt (record_type = 0x02 battery monitor)
//  [0-1]  tension batterie  (uint16, 10mV)
//  [2-3]  raison alarme
//  [4-5]  tension auxiliaire ou temperature
//  [6-8]  courant (int24, 1mA)
//  [9-11] Ah consommes (int24, 100mAh)
//  [12-13] SOC (uint16, 10 = 0.1%)
//  [14-15] TTG (uint16, minutes)
// ============================================================
void VictronBle::_parseSmartshunt(const uint8_t* d) {
    // Courant (int24, 1mA)
    int32_t raw_i = (int32_t)d[6] | ((int32_t)d[7] << 8) | ((int32_t)d[8] << 16);
    if (raw_i & 0x800000) raw_i |= 0xFF000000;
    _smartshunt.current = raw_i * 0.001f;

    // Ah consommes (int24, 100mAh)
    int32_t raw_ah = (int32_t)d[9] | ((int32_t)d[10] << 8) | ((int32_t)d[11] << 16);
    if (raw_ah & 0x800000) raw_ah |= 0xFF000000;
    _smartshunt.consumed_ah = raw_ah * 0.1f;

    // SOC
    uint16_t raw_soc = (uint16_t)d[12] | ((uint16_t)d[13] << 8);
    if (raw_soc != 0xFFFF) _smartshunt.soc = raw_soc * 0.01f;

    // TTG
    uint16_t ttg = (uint16_t)d[14] | ((uint16_t)d[15] << 8);
    _smartshunt.time_to_go = (ttg == 0xFFFF) ? -1.0f : (float)ttg;

    // Alarme
    _smartshunt.alarm_reason = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
    _smartshunt.alarm        = (_smartshunt.alarm_reason != 0);

    _smartshunt.data_valid     = true;
    _smartshunt.last_update_ms = millis();

    Serial.printf("[SmartShunt] I:%.3fA  SOC:%.1f%%  TTG:%.0fmin  Conso:%.1fAh\n",
                  _smartshunt.current, _smartshunt.soc,
                  _smartshunt.time_to_go, _smartshunt.consumed_ah);
}

// ============================================================
//  _parseSolarCharger() - SmartSolar MPPT
// ============================================================
void VictronBle::_parseSolarCharger(const uint8_t* d) {
    _mppt.device_state    = d[0];
    _mppt.charger_error   = d[1];
    _mppt.battery_voltage = ((uint16_t)d[2] | ((uint16_t)d[3] << 8)) * 0.01f;
    _mppt.battery_current = ((int16_t)(d[4] | (d[5] << 8)))           * 0.1f;
    _mppt.yield_today     = ((uint16_t)d[6] | ((uint16_t)d[7] << 8)) * 10.0f;
    _mppt.pv_voltage      = ((uint16_t)d[8] | ((uint16_t)d[9] << 8)) * 0.01f;
    _mppt.charge_power    = (float)((uint16_t)d[10] | ((uint16_t)d[11] << 8));
    _mppt.pv_power        = _mppt.pv_voltage * _mppt.battery_current;
    _mppt.data_valid      = true;
    _mppt.last_update_ms  = millis();

    Serial.printf("[SmartSolar] %s  Vbat:%.2fV  I:%.1fA  Pch:%.0fW  Vpv:%.1fV  Wh:%d\n",
                  _stateToString(_mppt.device_state),
                  _mppt.battery_voltage, _mppt.battery_current,
                  _mppt.charge_power, _mppt.pv_voltage, (int)_mppt.yield_today);
}

// ============================================================
//  _parseBatteryMonitor() - SmartBMV 712
// ============================================================
void VictronBle::_parseBatteryMonitor(const uint8_t* d) {
    uint16_t raw_v = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
    if (raw_v != 0xFFFF) _bmv.battery_voltage = raw_v * 0.01f;

    _bmv.alarm_reason = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
    _bmv.alarm        = (_bmv.alarm_reason != 0);

    uint16_t raw_aux = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
    _bmv.aux_is_temperature = (raw_aux & 0x8000) != 0;
    uint16_t aux_val = raw_aux & 0x7FFF;
    if (aux_val != 0x7FFF) {
        if (_bmv.aux_is_temperature)
            _bmv.aux_temperature = (aux_val * 0.01f) - 273.15f;
        else
            _bmv.aux_voltage = aux_val * 0.01f;
    }

    int32_t raw_i = (int32_t)d[6] | ((int32_t)d[7] << 8) | ((int32_t)d[8] << 16);
    if (raw_i & 0x800000) raw_i |= 0xFF000000;
    _bmv.battery_current = raw_i * 0.001f;

    int32_t raw_ah = (int32_t)d[9] | ((int32_t)d[10] << 8) | ((int32_t)d[11] << 16);
    if (raw_ah & 0x800000) raw_ah |= 0xFF000000;
    _bmv.consumed_ah = raw_ah * 0.1f;

    uint16_t raw_soc = (uint16_t)d[12] | ((uint16_t)d[13] << 8);
    if (raw_soc != 0xFFFF) _bmv.soc = raw_soc * 0.01f;

    uint16_t ttg = (uint16_t)d[14] | ((uint16_t)d[15] << 8);
    _bmv.time_to_go = (ttg == 0xFFFF) ? -1.0f : (float)ttg;

    _bmv.battery_power = _bmv.battery_voltage * _bmv.battery_current;

    if      (_bmv.battery_current >  0.5f) _bmv.trend = +1;
    else if (_bmv.battery_current < -0.5f) _bmv.trend = -1;
    else                                    _bmv.trend =  0;

    _bmv.relay_state = (_bmv.alarm_reason & 0x0040) != 0;

    _bmv.data_valid     = true;
    _bmv.last_update_ms = millis();

    Serial.printf("[SmartBMV] V:%.2fV  I:%.3fA  P:%.0fW  SOC:%.1f%%  TTG:%.0fmin  Conso:%.1fAh\n",
                  _bmv.battery_voltage, _bmv.battery_current,
                  _bmv.battery_power,   _bmv.soc,
                  _bmv.time_to_go,      _bmv.consumed_ah);
}

// ============================================================
//  _parseBatteryProtect()
//  Octets dechiffres (format simplifie) :
//  [0]    etat appareil
//  [1-2]  tension entree (uint16, 10mV)
//  [3-4]  tension sortie (uint16, 10mV)
//  [5-6]  raison alarme (uint16)
// ============================================================
void VictronBle::_parseBatteryProtect(const uint8_t* d) {
    _bp.device_state   = d[0];
    uint16_t vin  = (uint16_t)d[1] | ((uint16_t)d[2] << 8);
    uint16_t vout = (uint16_t)d[3] | ((uint16_t)d[4] << 8);
    if (vin  != 0xFFFF) _bp.input_voltage  = vin  * 0.01f;
    if (vout != 0xFFFF) _bp.output_voltage = vout * 0.01f;
    _bp.alarm_reason = (uint16_t)d[5] | ((uint16_t)d[6] << 8);
    _bp.alarm        = (_bp.alarm_reason != 0);
    _bp.data_valid     = true;
    _bp.last_update_ms = millis();

    Serial.printf("[BatteryProtect] Etat:%d  Vin:%.2fV  Vout:%.2fV  Alarme:%s\n",
                  _bp.device_state, _bp.input_voltage, _bp.output_voltage,
                  _bp.alarm ? "OUI" : "Non");
}

// ============================================================
//  _parseOrionSmart()
//  Octets dechiffres :
//  [0]    etat appareil
//  [1-2]  tension entree (uint16, 10mV)
//  [3-4]  tension sortie (uint16, 10mV)
//  [5-6]  courant sortie (int16, 10mA)
// ============================================================
void VictronBle::_parseOrionSmart(const uint8_t* d) {
    _orion.device_state = d[0];
    uint16_t vin  = (uint16_t)d[1] | ((uint16_t)d[2] << 8);
    uint16_t vout = (uint16_t)d[3] | ((uint16_t)d[4] << 8);
    int16_t  iout = (int16_t)((uint16_t)d[5] | ((uint16_t)d[6] << 8));
    if (vin  != 0xFFFF) _orion.input_voltage  = vin  * 0.01f;
    if (vout != 0xFFFF) _orion.output_voltage = vout * 0.01f;
    if (iout != (int16_t)0x7FFF) _orion.output_current = iout * 0.01f;
    _orion.output_power    = _orion.output_voltage * _orion.output_current;
    _orion.data_valid      = true;
    _orion.last_update_ms  = millis();

    Serial.printf("[OrionSmart] Etat:%d  Vin:%.2fV  Vout:%.2fV  I:%.2fA  P:%.0fW\n",
                  _orion.device_state, _orion.input_voltage,
                  _orion.output_voltage, _orion.output_current, _orion.output_power);
}

// ============================================================
//  _hexToBytes()
// ============================================================
void VictronBle::_hexToBytes(const char* hexStr, uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char hi = hexStr[i * 2];
        char lo = hexStr[i * 2 + 1];
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            if (c >= 'A' && c <= 'F') return 10 + c - 'A';
            return 0;
        };
        bytes[i] = (hexVal(hi) << 4) | hexVal(lo);
    }
}

// ============================================================
//  _stateToString()
// ============================================================
const char* VictronBle::_stateToString(uint8_t state) {
    switch (state) {
        case 0:   return "Off";
        case 2:   return "Defaut";
        case 3:   return "Bulk";
        case 4:   return "Absorption";
        case 5:   return "Float";
        case 7:   return "Egal.";
        case 9:   return "Charge";
        case 11:  return "Ext.ctrl";
        case 245: return "Demarr.";
        default:  return "Inconnu";
    }
}
