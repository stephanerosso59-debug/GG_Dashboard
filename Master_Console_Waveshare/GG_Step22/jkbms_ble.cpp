/*
 * jkbms_ble.cpp — Client BLE GATT JK-BMS ON-DEMAND
 *
 * Protocole V5.11 / JK02_32S
 * Offsets basés sur esphome-jk-bms (syssi)
 *
 * Trame 300 bytes, header 0x55 0xAA 0xEB 0x90
 */
#include "jkbms_ble.h"
#include "bluetooth_config.h"
#include "user_config.h"
#include <NimBLEDevice.h>

JkBmsData jkbmsData = {};

// ── UUIDs ─────────────────────────────────────────────────
static const char* JKBMS_SERVICE_UUID_STR = "0000ffe0-0000-1000-8000-00805f9b34fb";
static const char* JKBMS_CHAR_UUID_STR    = "0000ffe1-0000-1000-8000-00805f9b34fb";

// ── Commandes (tirées de esphome-jk-bms) ─────────────────
static const uint8_t CMD_READ_DEVICE_INFO[] = {
    0xAA, 0x55, 0x90, 0xEB, 0x97, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x11
};

static const uint8_t CMD_READ_CELL_INFO[] = {
    0xAA, 0x55, 0x90, 0xEB, 0x96, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10
};

// ── États ─────────────────────────────────────────────────
enum JkState {
    ST_IDLE,          // pas actif, pas connecté
    ST_CONNECTING,    // tentative en cours
    ST_CONNECTED,     // connecté + polling
    ST_DISCONNECTING  // déconnexion demandée
};

static JkState _state = ST_IDLE;
static bool    _active_requested = false;   // true = l'UI veut des données

static NimBLEClient*         _client   = nullptr;
static NimBLERemoteCharacteristic* _char = nullptr;

static uint32_t _last_connect_attempt_ms = 0;
static uint32_t _last_poll_ms            = 0;
static uint32_t _last_frame_rx_ms        = 0;

static const uint32_t RECONNECT_INTERVAL_MS = 10000;  // 10s entre tentatives
static const uint32_t POLL_INTERVAL_MS      = 3000;   // demande toutes 3s

// Buffer trame
static uint8_t  _rx_buf[320];
static size_t   _rx_len = 0;
static bool     _rx_syncing = false;

// ── Décodage little-endian ────────────────────────────────
static inline uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline int16_t read_i16(const uint8_t* p) {
    return (int16_t)read_u16(p);
}
static inline uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int32_t read_i32(const uint8_t* p) {
    return (int32_t)read_u32(p);
}

// ── Debug : hex dump ───────────────────────────────────────
static void debug_dump_frame(const uint8_t* f, size_t len) {
    Serial0.printf("[JKBMS] Trame %d bytes (type 0x%02X):\n", len, f[4]);
    for (size_t i = 0; i < len; i += 16) {
        Serial0.printf("  %03d: ", (int)i);
        for (size_t j = 0; j < 16 && (i+j) < len; j++) {
            Serial0.printf("%02X ", f[i+j]);
        }
        Serial0.println();
    }
}

// ── Parse trame type 0x02 (cell info) — OFFSETS JK02_32S CONFIRMES ─
// Offsets validés par dump trame réelle (firmware V11.287)
static void parse_cell_info_frame(const uint8_t* f, size_t len) {
    if (len < 300) {
        Serial0.printf("[JKBMS] Trame trop courte (%d)\n", len);
        return;
    }

    // Premier parsing : dump pour debug
    static bool first_frame_dumped = false;
    if (!first_frame_dumped) {
        Serial0.println("[JKBMS] === DUMP PREMIERE TRAME ===");
        debug_dump_frame(f, 300);
        first_frame_dumped = true;
    }

    // ═══ Cellules : offset 6, 32 × uint16 LE en mV ═══
    int active = 0;
    float sum_v = 0;
    float vmin = 99.0f, vmax = 0.0f;
    int imin = 0, imax = 0;
    for (int i = 0; i < 32; i++) {
        uint16_t mv = read_u16(f + 6 + i * 2);
        float v = mv * 0.001f;
        jkbmsData.cells_v[i] = v;
        if (mv > 500) {
            sum_v += v;
            if (v < vmin) { vmin = v; imin = i; }
            if (v > vmax) { vmax = v; imax = i; }
            active++;
        }
    }
    jkbmsData.cell_count     = active;
    jkbmsData.cell_v_avg     = active > 0 ? sum_v / active : 0;
    jkbmsData.cell_v_diff    = (vmax > vmin) ? (vmax - vmin) : 0;
    jkbmsData.cell_v_max_idx = imax;
    jkbmsData.cell_v_min_idx = imin;

    // ═══ Température MOSFET : offset 144, i16 × 0.1°C ═══
    jkbmsData.temp_mos_c = read_i16(f + 144) * 0.1f;

    // ═══ Tension batterie totale : offset 150, uint32 LE mV ═══
    jkbmsData.voltage = read_u32(f + 150) * 0.001f;

    // ═══ Courant batterie : offset 158, int32 LE mA (neg=decharge) ═══
    jkbmsData.current = read_i32(f + 158) * 0.001f;

    // Puissance = V × I
    jkbmsData.power = jkbmsData.voltage * jkbmsData.current;

    // ═══ Températures batterie : offsets 162, 164 (i16 × 0.1°C) ═══
    jkbmsData.temp_t1_c = read_i16(f + 162) * 0.1f;
    jkbmsData.temp_t2_c = read_i16(f + 164) * 0.1f;

    // ═══ Courant équilibrage : offset 166, i16 mA ═══
    jkbmsData.balance_current = read_i16(f + 166) * 0.001f;

    // ═══ Capacité restante : offset 174, uint32 LE mAh ═══
    jkbmsData.capacity_remain_ah = read_u32(f + 174) * 0.001f;

    // ═══ Capacité totale : offset 178, uint32 LE mAh ═══
    jkbmsData.capacity_total_ah = read_u32(f + 178) * 0.001f;

    // ═══ Cycles : offset 182, uint32 LE ═══
    jkbmsData.cycle_count = read_u32(f + 182);

    // ═══ Capacité cumulée cyclée : offset 186, uint32 LE mAh ═══
    jkbmsData.cycle_capacity_ah = read_u32(f + 186) * 0.001f;

    // ═══ SOC : offset 190, uint8 % ═══
    jkbmsData.soc_pct = f[190];

    // ═══ Uptime : offset 194, uint32 LE secondes ═══
    jkbmsData.uptime_s = read_u32(f + 194);

    // ═══ Charge MOS : offset 198 (uint8) ═══
    jkbmsData.charging_on    = (f[198] != 0);
    // ═══ Discharge MOS : offset 199 (uint8) ═══
    jkbmsData.discharging_on = (f[199] != 0);
    // ═══ Balance actif : offset 200 (uint8) ═══
    jkbmsData.balancing_on   = (f[200] != 0);

    jkbmsData.valid = true;
    jkbmsData.last_update_ms = millis();

    Serial0.printf("[JKBMS] V=%.2fV I=%+.2fA SOC=%.0f%% %dS Cap=%.0f/%.0fAh TMOS=%.1fC T1=%.1fC Cyc=%lu\n",
        jkbmsData.voltage, jkbmsData.current, jkbmsData.soc_pct,
        jkbmsData.cell_count, jkbmsData.capacity_remain_ah, jkbmsData.capacity_total_ah,
        jkbmsData.temp_mos_c, jkbmsData.temp_t1_c, (unsigned long)jkbmsData.cycle_count);
}

// ── Callback notification (reçoit les chunks) ─────────────
static void notify_cb(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool isNotify) {
    _last_frame_rx_ms = millis();

    // Header 0x55 0xAA 0xEB 0x90
    if (!_rx_syncing && len >= 4
        && data[0] == 0x55 && data[1] == 0xAA
        && data[2] == 0xEB && data[3] == 0x90) {
        _rx_syncing = true;
        _rx_len = 0;
        Serial0.printf("[JKBMS] Debut trame type 0x%02X\n", data[4]);
    }

    if (_rx_syncing && (_rx_len + len) <= sizeof(_rx_buf)) {
        memcpy(_rx_buf + _rx_len, data, len);
        _rx_len += len;

        if (_rx_len >= 300) {
            uint8_t frame_type = _rx_buf[4];
            uint8_t sum = 0;
            for (int i = 0; i < 299; i++) sum += _rx_buf[i];
            bool crc_ok = (sum == _rx_buf[299]);

            Serial0.printf("[JKBMS] Trame complete type=0x%02X crc=%s (calc=0x%02X attendu=0x%02X)\n",
                frame_type, crc_ok ? "OK" : "FAIL", sum, _rx_buf[299]);

            if (crc_ok && frame_type == 0x02) {
                parse_cell_info_frame(_rx_buf, _rx_len);
            } else if (!crc_ok) {
                Serial0.println("[JKBMS] CRC fail - trame ignoree");
            }

            _rx_syncing = false;
            _rx_len = 0;
        }
    } else if (_rx_syncing && (_rx_len + len) > sizeof(_rx_buf)) {
        Serial0.println("[JKBMS] Buffer overflow - reset");
        _rx_syncing = false;
        _rx_len = 0;
    }
}

// ── Callback connexion ────────────────────────────────────
class JkClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* c) override {
        Serial0.println("[JKBMS] >>> Connecte");
    }
    void onDisconnect(NimBLEClient* c, int reason) override {
        Serial0.println("[JKBMS] <<< Deconnecte");
        _state = ST_IDLE;
        jkbmsData.valid = false;
    }
};
static JkClientCallbacks _client_cb;

// ── Tentative de connexion ────────────────────────────────
static bool try_connect() {
    Serial0.printf("[JKBMS] Connexion %s...\n", userConfig.jkbms_mac);

    if (!_client) {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(&_client_cb, false);
    }

    NimBLEAddress addr(userConfig.jkbms_mac, BLE_ADDR_PUBLIC);  // NimBLE 2.x : type obligatoire
    if (!_client->connect(addr)) {
        Serial0.println("[JKBMS] Echec connect()");
        return false;
    }

    NimBLERemoteService* svc = _client->getService(JKBMS_SERVICE_UUID_STR);
    if (!svc) {
        Serial0.println("[JKBMS] Service 0xFFE0 introuvable");
        _client->disconnect();
        return false;
    }

    _char = svc->getCharacteristic(JKBMS_CHAR_UUID_STR);
    if (!_char) {
        Serial0.println("[JKBMS] Char 0xFFE1 introuvable");
        _client->disconnect();
        return false;
    }

    if (_char->canNotify()) {
        _char->subscribe(true, notify_cb);
    } else {
        Serial0.println("[JKBMS] Pas de notify");
        _client->disconnect();
        return false;
    }

    Serial0.println("[JKBMS] Envoi CMD device info...");
    _char->writeValue((uint8_t*)CMD_READ_DEVICE_INFO, sizeof(CMD_READ_DEVICE_INFO), false);
    delay(150);

    Serial0.println("[JKBMS] Envoi CMD cell info...");
    _char->writeValue((uint8_t*)CMD_READ_CELL_INFO, sizeof(CMD_READ_CELL_INFO), false);

    _last_poll_ms = millis();
    _last_frame_rx_ms = millis();
    return true;
}

// ── Déconnexion propre ────────────────────────────────────
static void do_disconnect() {
    Serial0.println("[JKBMS] Deconnexion demandee");
    if (_client && _client->isConnected()) {
        _client->disconnect();
    }
    _state = ST_IDLE;
    jkbmsData.valid = false;
}

// ============================================================
//  API
// ============================================================
void jkbms_init() {
    jkbmsData = {};
    _state = ST_IDLE;
    _active_requested = false;
    Serial0.println("[JKBMS] Init OK (inactif, en attente de demande UI)");
}

void jkbms_request_activate() {
    if (!_active_requested) {
        Serial0.println("[JKBMS] >>> ACTIVATION demandee par UI");
    }
    _active_requested = true;
}

void jkbms_request_deactivate() {
    if (_active_requested) {
        Serial0.println("[JKBMS] <<< DESACTIVATION demandee par UI");
    }
    _active_requested = false;
}

bool jkbms_is_active_requested() { return _active_requested; }

void jkbms_update() {
    uint32_t now = millis();

    // ── CAS 1 : UI ne veut pas de données ────────────────────
    if (!_active_requested) {
        if (_state == ST_CONNECTED || _state == ST_CONNECTING) {
            do_disconnect();
        }
        return;
    }

    // ── CAS 2 : UI veut des données ──────────────────────────
    
    // Si déconnecté : essayer de se reconnecter périodiquement
    if (_state == ST_IDLE) {
        if (now - _last_connect_attempt_ms >= RECONNECT_INTERVAL_MS
            || _last_connect_attempt_ms == 0) {
            _last_connect_attempt_ms = now;
            _state = ST_CONNECTING;
            if (try_connect()) {
                _state = ST_CONNECTED;
            } else {
                _state = ST_IDLE;
            }
        }
        return;
    }

    // Si connecté : poll périodique
    if (_state == ST_CONNECTED) {
        if (!_client || !_client->isConnected()) {
            _state = ST_IDLE;
            jkbmsData.valid = false;
            return;
        }

        if (now - _last_poll_ms >= POLL_INTERVAL_MS) {
            _last_poll_ms = now;
            if (_char) {
                _char->writeValue((uint8_t*)CMD_READ_CELL_INFO,
                    sizeof(CMD_READ_CELL_INFO), false);
            }
        }

        // Timeout : pas de trame depuis 20s = reconnect
        if ((now - _last_frame_rx_ms) > 20000) {
            Serial0.println("[JKBMS] Timeout trames - reconnect");
            do_disconnect();
        }
    }
}

bool jkbms_is_connected() {
    return _state == ST_CONNECTED && _client && _client->isConnected();
}

bool jkbms_is_available() {
    return jkbmsData.valid && (millis() - jkbmsData.last_update_ms) < 30000;
}

const char* jkbms_state_str() {
    switch (_state) {
        case ST_IDLE:          return _active_requested ? "Attente..." : "Inactif";
        case ST_CONNECTING:    return "Connexion...";
        case ST_CONNECTED:     return jkbmsData.valid ? "Actif" : "Lecture...";
        case ST_DISCONNECTING: return "Deconnexion";
    }
    return "?";
}
