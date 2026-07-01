/*
 * heating_ble.cpp — Client BLE GATT chauffage diesel chinois
 * 
 * Protocole AA55 (le plus courant sur Nordkapp/Hcalory/Vevor/BYD) :
 *   Frame : AA 55 [cmd] [data...] [checksum]
 *   Response : AA 55 [cmd_echo] [data...] [checksum]
 * 
 * Commandes courantes (d'apres AirHeater-BLE) :
 *   0xA1 : Power toggle (ON/OFF)
 *   0xA4 : Set level 1-10
 *   0xA5 : Set temperature 8-36°C
 *   0xA6 : Set mode (0=level, 1=temp)
 *   0xA7 : Query status (→ 40 bytes response)
 * 
 * Service UUID  : 0000ffe0-0000-1000-8000-00805f9b34fb (même que JKBMS !)
 * Write char    : 0000ffe1-0000-1000-8000-00805f9b34fb
 * Notify char   : 0000ffe1-0000-1000-8000-00805f9b34fb
 */
#include "heating_ble.h"
#include "bluetooth_config.h"
#include "user_config.h"
#include "sd_card.h"
#include <NimBLEDevice.h>

HeatingData heatingData = {};

// UUIDs
// UUIDs du Nordkapp (confirmés par discovery BLE)
static const char* HEATER_SERVICE_UUID  = "0000181a-0000-1000-8000-00805f9b34fb";  // 16bit: 0x181a
static const char* HEATER_WRITE_UUID    = "00003a01-0000-1000-8000-00805f9b34fb";  // Write
static const char* HEATER_NOTIFY_UUID   = "00003a00-0000-1000-8000-00805f9b34fb";  // Read+Notify

// État
enum HeaterState { HS_IDLE, HS_CONNECTING, HS_CONNECTED };
static HeaterState _state = HS_IDLE;
static bool _active_requested = false;
static NimBLEClient* _client = nullptr;
static NimBLERemoteCharacteristic* _char = nullptr;

static uint32_t _last_connect_ms = 0;
static uint32_t _last_poll_ms    = 0;
static uint32_t _last_rx_ms      = 0;
static const uint32_t RECONNECT_MS = 30000;  // 30s entre tentatives (au lieu de 10s)
static const uint32_t POLL_MS      = 5000;

// ── Checksum (somme des bytes) ────────────────────────────
static uint8_t calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return sum;
}

// ── Build commande AA55 ────────────────────────────────────
static void build_cmd(uint8_t cmd, uint8_t d1, uint8_t d2, uint8_t* out) {
    // Frame 8 bytes : AA 55 cmd d1 d2 00 00 cksum
    out[0] = 0xAA;
    out[1] = 0x55;
    out[2] = cmd;
    out[3] = d1;
    out[4] = d2;
    out[5] = 0x00;
    out[6] = 0x00;
    out[7] = calc_checksum(out, 7);
}

// ── Parse trame Nordkapp (52 bytes, header AA 09 FF) ──────
// Format reverse-engineered avec capture nRF Connect :
//   [0..1]   AA 09        : magic header
//   [2]      FF           : constant
//   [3]      XX           : counter / sequence (croit a chaque trame)
//   [4..7]   00 00 00 F2  : header constant
//   [8]      State1       : 0x51/0x55 — état alimentation/run
//   [9]      State2       : 0xC0/0xC4/0xCC — état moteur (?)
//   [10]     Tension bat  : x0.1 V (ex 0x86=134 → 13.4V, 0x7D=125 → 12.5V)
//   [11..13] 00 00 00     : reserved
//   [14]     Mode/Step    : 0x0E/0x0F/0x10 — étape allumage
//   [15]     01           : constant
//   [16]     Temp room    : x0.1 °C (ex 0xEB=235 → 23.5°C, 0xF0=240 → 24.0°C)
//   [17]     01           : constant
//   [18..19] 00 00        : reserved
//   [20..21] Fan_RPM u16  : vitesse ventilateur
//   [22..23] Pump u16     : débit pompe carburant (~ 0.001 Hz/unit)
//   [24..29] F8 7F F8 7F 00 00 : sentinels
//   [30..37] Device ID    : 0E C0 8F AD 67 49 00 00 (constant)
//   [38..40] FW info      : A5 03 05 (constant)
//   [41]     Run hours    : 0x1C → 0x20 (compteur heures de fonctionnement)
//   [42..47] 01 90 00 00 00 00 : constants
//   [48..51] CRC32        : checksum (varie par trame)

static void parse_status(const uint8_t* f, size_t len) {
    if (len < 52) {
        Serial0.printf("[Heater] Trame trop courte (%d, attendu 52)\n", len);
        return;
    }
    if (f[0] != 0xAA || f[1] != 0x09 || f[2] != 0xFF) {
        Serial0.printf("[Heater] Magic header invalide: %02X %02X %02X\n",
            f[0], f[1], f[2]);
        return;
    }
    
    // Decodage des champs
    uint8_t  counter      = f[3];
    uint8_t  state1       = f[8];   // alim/allumeur (55/51/15)
    uint8_t  state2       = f[9];   // phase moteur (40/C0/C4/CC)
    float    voltage      = f[10] * 0.1f;
    uint8_t  step         = f[14];
    float    temp_room_c  = f[16] * 0.1f;
    uint16_t fan_rpm      = (uint16_t)f[20] | ((uint16_t)f[21] << 8);
    uint16_t pump_raw     = (uint16_t)f[22] | ((uint16_t)f[23] << 8);
    uint8_t  hours        = f[41];
    
    // Détection état :
    //   state2 = 0x40 -> OFF
    //   state2 = 0xC0/C4/CC -> ON (différents sub-états)
    bool running = (state2 != 0x40);
    
    // État textuel (pour UI)
    uint8_t running_state = 0;  // par défaut OFF
    if (state2 == 0x40) {
        running_state = 0;  // OFF
    } else if (fan_rpm == 0 && pump_raw == 0) {
        running_state = 1;  // Préparation (avant allumage)
    } else if (state2 == 0xC0 && pump_raw > 0) {
        running_state = 2;  // Démarrage / pré-chauffe
    } else if (state2 == 0xC4) {
        running_state = 3;  // Chauffe active
    } else if (state2 == 0xCC) {
        running_state = 3;  // Chauffe stable
    } else {
        running_state = 4;  // Refroidissement
    }
    
    heatingData.running        = running;
    heatingData.running_state  = running_state;
    heatingData.running_step   = step;
    heatingData.error_code     = 0;
    heatingData.room_temp_c    = temp_room_c;
    heatingData.case_temp_c    = temp_room_c;     // pas de temp boitier dispo
    heatingData.set_level      = 0;
    heatingData.set_temp_c     = 0;
    heatingData.supply_voltage = voltage;
    heatingData.altitude_m     = 0;
    heatingData.fuel_pump_hz   = pump_raw / 100;
    heatingData.fan_rpm        = fan_rpm;
    heatingData.running_mode   = 0;
    
    heatingData.valid = true;
    heatingData.last_update_ms = millis();
    
    // Description état
    const char* state_str = "?";
    switch (running_state) {
        case 0: state_str = "OFF";       break;
        case 1: state_str = "PREP";      break;
        case 2: state_str = "DEMARRAGE"; break;
        case 3: state_str = "RUN";       break;
        case 4: state_str = "COOLDOWN";  break;
    }
    
    Serial0.printf("[Heater] %s | V=%.1fV temp=%.1f°C fan=%d pump=%d hr=%d (s1=%02X s2=%02X step=%02X cnt=%d)\n",
        state_str, voltage, temp_room_c, fan_rpm, pump_raw, hours,
        state1, state2, step, counter);
}

// ── Callback notification ──────────────────────────────────
static void notify_cb(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool isNotify) {
    _last_rx_ms = millis();
    
    // Dump hex des 5 premières trames pour debug
    static int dump_count = 0;
    if (dump_count < 5) {
        Serial0.printf("[Heater] === Trame #%d recue (%d bytes) ===\n", dump_count + 1, len);
        Serial0.flush();
        for (size_t i = 0; i < len; i++) {
            Serial0.printf("%02X ", data[i]);
            if ((i+1) % 16 == 0) {
                Serial0.println();
                Serial0.flush();
            }
        }
        if (len % 16 != 0) Serial0.println();
        Serial0.flush();
        dump_count++;
    }
    
    // ═══ LOG SUR SD CARD (si dispo) ═══════════════════════════
    // Format CSV : timestamp_ms, state1, state2, step, V_bat, temp, hex_complet
    if (sdCard.mounted && len > 0) {
        // Nom de fichier base sur le boot time pour avoir un fichier par session
        static char session_filename[40] = "";
        if (session_filename[0] == 0) {
            // Premiere fois : creer le nom + entete
            snprintf(session_filename, sizeof(session_filename),
                     "/heater_%lu.csv", (unsigned long)(millis() / 1000));
            sd_card_write_file(session_filename,
                "timestamp_ms,len,b07_state1,b08_state2,b09_state3,b14_step,"
                "b10_vbat_x10,b16_temp_x10,b20_fan_low,b21_fan_high,"
                "b22_pump_low,b23_pump_high,b41_hours,hex_full\n");
            Serial0.printf("[Heater-SD] Log session: %s\n", session_filename);
        }
        
        // Construction de la ligne CSV
        char line[256];
        int n = snprintf(line, sizeof(line), "%lu,%u,",
                         (unsigned long)millis(), (unsigned)len);
        
        if (len >= 52) {
            // Decoded fields utiles (pour analyse rapide)
            n += snprintf(line + n, sizeof(line) - n,
                          "%02X,%02X,%02X,%02X,%u,%u,%u,%u,%u,%u,%u,",
                          data[7], data[8], data[9], data[14],
                          data[10], data[16],
                          data[20], data[21],
                          data[22], data[23],
                          data[41]);
        } else {
            n += snprintf(line + n, sizeof(line) - n, ",,,,,,,,,,,");
        }
        
        // Hex complet de la trame
        for (size_t i = 0; i < len && (n + 3) < (int)sizeof(line); i++) {
            n += snprintf(line + n, sizeof(line) - n, "%02X", data[i]);
        }
        if ((n + 1) < (int)sizeof(line)) {
            line[n++] = '\n';
            line[n] = 0;
        }
        
        sd_card_append_file(session_filename, line);
    }
    
    // Format Nordkapp custom (AA 09 FF, 52 bytes) - protocole reverse-engineered
    if (len >= 52 && data[0] == 0xAA && data[1] == 0x09 && data[2] == 0xFF) {
        parse_status(data, len);
    }
    // Format AA55 (chauffages basiques)
    else if (len >= 8 && data[0] == 0xAA && data[1] == 0x55) {
        Serial0.println("[Heater] Format AA55 detecte (proto basique)");
    }
    // Format ABBA
    else if (len >= 8 && data[0] == 0xAB && data[1] == 0xBA) {
        Serial0.println("[Heater] Format ABBA detecte (non implemente)");
    }
}

// ── Connexion ──────────────────────────────────────────────
class HeaterCb : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* c) override {
        Serial0.println("[Heater] >>> Connecte");
    }
    void onDisconnect(NimBLEClient* c, int reason) override {
        Serial0.println("[Heater] <<< Deconnecte");
        _state = HS_IDLE;
        heatingData.valid = false;
    }
};
static HeaterCb _cb;

static bool try_connect() {
    Serial0.printf("[Heater] Connexion %s...\n", userConfig.heating_mac);
    
    // CRITIQUE : arrêter le scan BLE avant de se connecter
    // (NimBLE ne peut pas scanner et connecter en même temps)
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan && scan->isScanning()) {
        scan->stop();
        Serial0.println("[Heater] Scan arrete pour connexion");
        delay(100);  // laisse le scan s'arreter proprement
    }
    
    if (!_client) {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(&_cb, false);
    }
    
    // Tenter avec adresse PUBLIC d'abord (typique pour modules Tuya/clones)
    // Si echec on essaiera RANDOM
    static int addr_type_attempt = 0;
    uint8_t addr_type = (addr_type_attempt % 2 == 0) ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;
    addr_type_attempt++;
    
    NimBLEAddress addr(userConfig.heating_mac, addr_type);
    Serial0.printf("[Heater] Tentative addr_type=%s\n",
        addr_type == BLE_ADDR_PUBLIC ? "PUBLIC" : "RANDOM");
    
    // Augmenter timeout connexion
    _client->setConnectionParams(12, 12, 0, 200);
    _client->setConnectTimeout(15000);  // 15s (NimBLE 2.x : en MS)
    
    if (!_client->connect(addr)) {
        Serial0.println("[Heater] connect() echec");
        return false;
    }
    
    Serial0.println("[Heater] >>> TCP connecte, discovery services...");
    Serial0.flush();
    delay(200);
    
    // Lister TOUS les services (pour debug)
    const std::vector<NimBLERemoteService*>& svcs = _client->getServices(true);
    int n = (int)svcs.size();
    Serial0.printf("[Heater] %d services decouverts:\n", n);
    Serial0.flush();

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            NimBLERemoteService* s = svcs[i];
            if (!s) continue;
            std::string uuid_str = s->getUUID().toString();
            Serial0.printf("  [%d] Service: %s\n", i, uuid_str.c_str());
            Serial0.flush();
            
            const std::vector<NimBLERemoteCharacteristic*>& chars = s->getCharacteristics(true);
            int nc = (int)chars.size();
            for (int j = 0; j < nc; j++) {
                NimBLERemoteCharacteristic* c = chars[j];
                if (!c) continue;
                Serial0.printf("      Char: %s props=%s%s%s%s\n",
                    c->getUUID().toString().c_str(),
                    c->canRead() ? "R" : "-",
                    c->canWrite() ? "W" : "-",
                    c->canWriteNoResponse() ? "w" : "-",
                    c->canNotify() ? "N" : "-");
                Serial0.flush();
            }
        }
    }
    delay(100);
    
    // ═══ Service Nordkapp confirmé : 0x181a ═══
    // Service: 0x181a (Environmental Sensing repurposed)
    //   Char 0x3a01 : WRITE (envoi commandes)
    //   Char 0x3a00 : READ + NOTIFY (réception status)
    NimBLERemoteService* svc = _client->getService(NimBLEUUID((uint16_t)0x181a));
    if (!svc) {
        // Fallback : essayer aussi via UUID complet
        svc = _client->getService("0000181a-0000-1000-8000-00805f9b34fb");
    }
    
    if (!svc) {
        Serial0.println("[Heater] Service 0x181a introuvable !");
        Serial0.flush();
        _client->disconnect();
        return false;
    }
    
    // Caractéristique pour ECRIRE les commandes
    _char = svc->getCharacteristic(NimBLEUUID((uint16_t)0x3a01));
    if (!_char) {
        Serial0.println("[Heater] Char 0x3a01 (write) introuvable");
        Serial0.flush();
        _client->disconnect();
        return false;
    }
    Serial0.println("[Heater] Write char 0x3a01 OK");
    
    // Caractéristique pour LIRE les notifications
    NimBLERemoteCharacteristic* notify_char = svc->getCharacteristic(NimBLEUUID((uint16_t)0x3a00));
    if (!notify_char) {
        Serial0.println("[Heater] Char 0x3a00 (notify) introuvable");
        Serial0.flush();
        _client->disconnect();
        return false;
    }
    Serial0.println("[Heater] Notify char 0x3a00 OK");
    
    if (notify_char->canNotify()) {
        notify_char->subscribe(true, notify_cb);
        Serial0.println("[Heater] Notify subscribed sur 0x3a00");
    }
    
    // Pour le moment on N'ENVOIE PAS de commande - on ECOUTE ce qui arrive
    // Le chauffage envoie peut-etre des trames spontanées
    Serial0.println("[Heater] >>> Ecoute des notifications (pas de commande envoyee)");
    Serial0.flush();
    
    _last_poll_ms = millis();
    _last_rx_ms   = millis();
    return true;
}

static void do_disconnect() {
    if (_client && _client->isConnected()) _client->disconnect();
    _state = HS_IDLE;
    heatingData.valid = false;
}

// ── API ────────────────────────────────────────────────────
void heating_init() {
    heatingData = {};
    _state = HS_IDLE;
    _active_requested = false;
    Serial0.println("[Heater] Init OK (inactif, attente demande UI)");
}

void heating_request_activate() {
    if (!_active_requested) Serial0.println("[Heater] >>> ACTIVATION UI");
    _active_requested = true;
}

void heating_request_deactivate() {
    if (_active_requested) Serial0.println("[Heater] <<< DESACTIVATION UI");
    _active_requested = false;
}

void heating_update() {
    uint32_t now = millis();
    
    if (!_active_requested) {
        if (_state == HS_CONNECTED || _state == HS_CONNECTING) do_disconnect();
        return;
    }
    
    if (_state == HS_IDLE) {
        if (now - _last_connect_ms >= RECONNECT_MS || _last_connect_ms == 0) {
            _last_connect_ms = now;
            _state = HS_CONNECTING;
            _state = try_connect() ? HS_CONNECTED : HS_IDLE;
        }
        return;
    }
    
    if (_state == HS_CONNECTED) {
        if (!_client || !_client->isConnected()) {
            _state = HS_IDLE;
            heatingData.valid = false;
            return;
        }
        
        // Pas de polling pour l'instant - on ECOUTE les notifications
        // (à activer une fois qu'on connait le bon format de commande)
        
        if (now - _last_rx_ms > 30000) {
            Serial0.println("[Heater] Timeout - reconnect");
            do_disconnect();
        }
    }
}

bool heating_is_connected() {
    return _state == HS_CONNECTED && _client && _client->isConnected();
}

bool heating_is_available() {
    return heatingData.valid && (millis() - heatingData.last_update_ms) < 30000;
}

const char* heating_state_str() {
    switch (_state) {
        case HS_IDLE:       return _active_requested ? "Attente..." : "Inactif";
        case HS_CONNECTING: return "Connexion...";
        case HS_CONNECTED:  return heatingData.valid ? "Actif" : "Lecture...";
    }
    return "?";
}

// ── Commandes ──────────────────────────────────────────────
static bool send_cmd(uint8_t cmd, uint8_t d1, uint8_t d2) {
    if (!heating_is_connected() || !_char) return false;
    uint8_t buf[8];
    build_cmd(cmd, d1, d2, buf);
    _char->writeValue(buf, 8, false);
    Serial0.printf("[Heater] CMD 0x%02X %02X %02X\n", cmd, d1, d2);
    return true;
}

void heating_set_power(bool on) {
    // Le power toggle est 0xA1 (pas de distinction on/off - toggle)
    send_cmd(0xA1, 0, 0);
}

void heating_set_level(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    send_cmd(0xA4, level, 0);
}

void heating_set_temp(uint8_t temp_c) {
    if (temp_c < 8) temp_c = 8;
    if (temp_c > 36) temp_c = 36;
    send_cmd(0xA5, temp_c, 0);
}

void heating_set_mode(uint8_t mode) {
    // 0 = level mode, 1 = temperature mode
    send_cmd(0xA6, mode ? 1 : 0, 0);
}
