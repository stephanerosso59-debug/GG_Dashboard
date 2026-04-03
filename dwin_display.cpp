/*
 * dwin_display.cpp - Implementation driver DWIN DGUS2
 */
#include "dwin_display.h"
#include <string.h>

DwinDisplay* DwinDisplay::instance = nullptr;
DwinDisplay  dwin;

// ============================================================
//  Constructeur
// ============================================================
DwinDisplay::DwinDisplay()
    : _rxLen(0), _lastPollMs(0), _current_page(0xFF), _touchCb(nullptr) {
    memset(&_lastEvent, 0, sizeof(_lastEvent));
    instance = this;
}

// ============================================================
//  begin()
// ============================================================
void DwinDisplay::begin() {
    DWIN_SERIAL.begin(DWIN_BAUD, SERIAL_8N1, DWIN_RX_PIN, DWIN_TX_PIN);
    delay(200);
    Serial.println("[DWIN] Initialisation...");
    showPage(1);
    setBacklight(100);
    Serial.println("[DWIN] Pret");
}

// ============================================================
//  update() - Appeler dans loop()
// ============================================================
void DwinDisplay::update() {
    if (millis() - _lastPollMs >= DWIN_POLL_MS) {
        _lastPollMs = millis();
        while (_readFrame()) {}
    }
}

// ============================================================
//  showPage()
// ============================================================
void DwinDisplay::showPage(uint8_t page_id) {
    if (_current_page == page_id) return;
    _current_page = page_id;
    // Trame changement de page DWIN : 5A A5 07 82 00 84 5A 01 [page] 00
    uint8_t data[] = {0x5A, 0x01, page_id, 0x00};
    _sendFrame(DWIN_CMD_WRITE_VP, 0x0084, data, sizeof(data));
    Serial.printf("[DWIN] Page -> %d\n", page_id);
}

// ============================================================
//  setBacklight()  0-100%
// ============================================================
void DwinDisplay::setBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint8_t data[] = {0x5A, percent};
    _sendFrame(DWIN_CMD_WRITE_REG, 0x0082, data, 2);
}

// ============================================================
//  buzz()
// ============================================================
void DwinDisplay::buzz(uint8_t ms) {
    writeVP_u16(DWIN_VP_BUZZER, ms);
}

// ============================================================
//  writeVP_u16()
// ============================================================
void DwinDisplay::writeVP_u16(uint16_t vp, uint16_t value) {
    uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    _sendFrame(DWIN_CMD_WRITE_VP, vp, data, 2);
}

// ============================================================
//  writeVP_i16()
// ============================================================
void DwinDisplay::writeVP_i16(uint16_t vp, int16_t value) {
    writeVP_u16(vp, (uint16_t)value);
}

// ============================================================
//  writeVP_u32()  (2 mots consecutifs)
// ============================================================
void DwinDisplay::writeVP_u32(uint16_t vp, uint32_t value) {
    uint8_t data[4] = {
        (uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),  (uint8_t)(value & 0xFF)
    };
    _sendFrame(DWIN_CMD_WRITE_VP, vp, data, 4);
}

// ============================================================
//  writeVP_text()  - Texte ASCII, padde de 0xFF
// ============================================================
void DwinDisplay::writeVP_text(uint16_t vp, const char* str, uint8_t max_chars) {
    uint8_t buf[32];
    memset(buf, 0xFF, sizeof(buf));
    uint8_t len = strlen(str);
    if (len > max_chars) len = max_chars;
    memcpy(buf, str, len);
    buf[len] = 0x00;  // Terminateur DWIN
    _sendFrame(DWIN_CMD_WRITE_VP, vp, buf, (max_chars % 2 == 0) ? max_chars : max_chars + 1);
}

// ============================================================
//  writeVP_icon()
// ============================================================
void DwinDisplay::writeVP_icon(uint16_t vp, uint16_t icon_id) {
    writeVP_u16(vp, icon_id);
}

// ============================================================
//  updateClock()
// ============================================================
void DwinDisplay::updateClock(uint8_t hour, uint8_t min, uint8_t sec) {
    writeVP_u16(DWIN_VP_CLOCK_HOUR, hour);
    writeVP_u16(DWIN_VP_CLOCK_MIN,  min);
    writeVP_u16(DWIN_VP_CLOCK_SEC,  sec);
}

// ============================================================
//  updateDate()
// ============================================================
void DwinDisplay::updateDate(const char* date_str) {
    writeVP_text(DWIN_VP_DATE_TEXT, date_str, 16);
}

// ============================================================
//  updateWifiStatus()
// ============================================================
void DwinDisplay::updateWifiStatus(bool connected, int8_t rssi_dbm, const char* ip) {
    uint16_t wifi_st = 0;
    if (connected) {
        wifi_st = (rssi_dbm > -65) ? 1 : 2;  // 1=bon 2=faible
    }
    writeVP_u16(DWIN_VP_WIFI_STATUS, wifi_st);
    writeVP_i16(DWIN_VP_WIFI_RSSI, (int16_t)rssi_dbm);
    writeVP_text(DWIN_VP_IP_TEXT, ip ? ip : "---", 16);
}

// ============================================================
//  updateBleStatus()
// ============================================================
void DwinDisplay::updateBleStatus(uint8_t status) {
    writeVP_u16(DWIN_VP_BLE_STATUS, status);
}

// ============================================================
//  updateWeather()
// ============================================================
void DwinDisplay::updateWeather(uint8_t day, uint8_t icon_id,
                                 float temp_max, float temp_min, uint8_t humidity) {
    uint16_t base = DWIN_VP_WTH0_ICON + (day * 4);
    writeVP_icon(base,     icon_id);
    writeVP_i16(base + 1, (int16_t)(temp_max * 10));
    writeVP_i16(base + 2, (int16_t)(temp_min * 10));
    writeVP_u16(base + 3, humidity);
}

// ============================================================
//  updateJkBms()
// ============================================================
void DwinDisplay::updateJkBms(float voltage, float current, float power,
                               float soc, float remain_ah, float temp_mos,
                               uint32_t cycles, bool balancing, uint8_t alarms) {
    writeVP_u16(DWIN_VP_BAT_SOC,       (uint16_t)soc);
    writeVP_u16(DWIN_VP_BAT_VOLTAGE,   (uint16_t)(voltage   * 100.0f));
    writeVP_i16(DWIN_VP_BAT_CURRENT,   (int16_t) (current   * 10.0f));
    writeVP_i16(DWIN_VP_BAT_POWER,     (int16_t) power);
    writeVP_u16(DWIN_VP_BAT_REMAIN_AH, (uint16_t)(remain_ah * 10.0f));
    writeVP_i16(DWIN_VP_BAT_TEMP_MOS,  (int16_t) (temp_mos  * 10.0f));
    writeVP_u16(DWIN_VP_BAT_CYCLES,    (uint16_t)(cycles & 0xFFFF));
    writeVP_u16(DWIN_VP_BAT_BALANCE,   balancing ? 1 : 0);
    writeVP_u16(DWIN_VP_BAT_ALARMS,    alarms);
}

// ============================================================
//  updateCells()
// ============================================================
void DwinDisplay::updateCells(float cell_min, float cell_max,
                               float cell_avg, float delta_mv, uint8_t cell_count) {
    writeVP_u16(DWIN_VP_CELL_MIN_V,    (uint16_t)(cell_min * 1000.0f));
    writeVP_u16(DWIN_VP_CELL_MAX_V,    (uint16_t)(cell_max * 1000.0f));
    writeVP_u16(DWIN_VP_CELL_AVG_V,    (uint16_t)(cell_avg * 1000.0f));
    writeVP_u16(DWIN_VP_CELL_DELTA_MV, (uint16_t)delta_mv);
    writeVP_u16(DWIN_VP_CELL_COUNT,    cell_count);
}

// ============================================================
//  updateMppt()
// ============================================================
void DwinDisplay::updateMppt(uint8_t state, float pv_voltage, float pv_power,
                              float bat_voltage, float charge_current,
                              float charge_power, float yield_today, uint8_t error) {
    writeVP_u16(DWIN_VP_MPPT_STATE,     state);
    writeVP_u16(DWIN_VP_MPPT_PV_VOLT,   (uint16_t)(pv_voltage      * 100.0f));
    writeVP_u16(DWIN_VP_MPPT_PV_POWER,  (uint16_t)pv_power);
    writeVP_u16(DWIN_VP_MPPT_BAT_VOLT,  (uint16_t)(bat_voltage     * 100.0f));
    writeVP_i16(DWIN_VP_MPPT_CHG_CURR,  (int16_t) (charge_current  * 10.0f));
    writeVP_u16(DWIN_VP_MPPT_CHG_PWR,   (uint16_t)charge_power);
    writeVP_u16(DWIN_VP_MPPT_YIELD_DAY, (uint16_t)yield_today);
    writeVP_u16(DWIN_VP_MPPT_ERROR,     error);
}

// ============================================================
//  updateShunt()
// ============================================================
void DwinDisplay::updateShunt(float voltage, float current, float power,
                               float soc, float ttg_min, float consumed_ah, bool alarm) {
    writeVP_u16(DWIN_VP_SHUNT_SOC,     (uint16_t)(soc     * 10.0f));
    writeVP_u16(DWIN_VP_SHUNT_VOLTAGE, (uint16_t)(voltage * 100.0f));
    writeVP_i16(DWIN_VP_SHUNT_CURRENT, (int16_t) (current * 100.0f));
    writeVP_i16(DWIN_VP_SHUNT_POWER,   (int16_t) power);
    writeVP_u16(DWIN_VP_SHUNT_TTG,     (ttg_min < 0) ? 0xFFFF : (uint16_t)ttg_min);
    writeVP_u16(DWIN_VP_SHUNT_AH_USED, (uint16_t)(consumed_ah * 10.0f));
    writeVP_u16(DWIN_VP_SHUNT_ALARM,   alarm ? 1 : 0);
}

// ============================================================
//  updateRelayStates()
// ============================================================
void DwinDisplay::updateRelayStates(bool l1, bool l2, bool l3,
                                     bool l4, bool l5, bool l6, bool tv) {
    uint16_t mask = 0;
    if (l1) mask |= (1 << 0);
    if (l2) mask |= (1 << 1);
    if (l3) mask |= (1 << 2);
    if (l4) mask |= (1 << 3);
    if (l5) mask |= (1 << 4);
    if (l6) mask |= (1 << 5);
    if (tv) mask |= (1 << 6);
    writeVP_u16(DWIN_VP_RELAY_STATE, mask);
}

// ============================================================
//  updateHeating()
// ============================================================
void DwinDisplay::updateHeating(bool on, float target_temp) {
    writeVP_u16(DWIN_VP_HEAT_STATE,  on ? 1 : 0);
    writeVP_u16(DWIN_VP_HEAT_TARGET, (uint16_t)(target_temp * 10.0f));
}

// ============================================================
//  updateSystemInfo()
// ============================================================
void DwinDisplay::updateSystemInfo(const char* ssid, const char* ip,
                                    const char* mac, int8_t rssi,
                                    uint32_t uptime_s, uint16_t free_heap_kb,
                                    float cpu_temp, const char* version,
                                    const char* chip_info,
                                    bool jkbms_ok, bool mppt_ok, bool shunt_ok) {
    writeVP_text(DWIN_VP_SYS_WIFI_SSID, ssid    ? ssid    : "---", 16);
    writeVP_text(DWIN_VP_SYS_WIFI_IP,   ip      ? ip      : "---", 16);
    writeVP_text(DWIN_VP_SYS_WIFI_MAC,  mac     ? mac     : "---", 16);
    writeVP_i16 (DWIN_VP_SYS_RSSI,      (int16_t)rssi);

    uint16_t uptime_h = uptime_s / 3600;
    uint8_t  uptime_m = (uptime_s % 3600) / 60;
    writeVP_u16(DWIN_VP_SYS_UPTIME_H, uptime_h);
    writeVP_u16(DWIN_VP_SYS_UPTIME_M, uptime_m);
    writeVP_u16(DWIN_VP_SYS_FREE_HEAP, free_heap_kb);
    writeVP_i16(DWIN_VP_SYS_CPU_TEMP,  (int16_t)(cpu_temp * 10.0f));

    writeVP_text(DWIN_VP_SYS_VERSION,   version   ? version   : "---", 16);
    writeVP_text(DWIN_VP_SYS_CHIP_INFO, chip_info ? chip_info : "---", 16);

    writeVP_u16(DWIN_VP_SYS_BLE_JKBMS, jkbms_ok ? 1 : 0);
    writeVP_u16(DWIN_VP_SYS_BLE_MPPT,  mppt_ok  ? 1 : 0);
    writeVP_u16(DWIN_VP_SYS_BLE_SHUNT, shunt_ok ? 1 : 0);
}

// ============================================================
//  _sendFrame()  - Construit et envoie une trame DWIN
// ============================================================
void DwinDisplay::_sendFrame(uint8_t cmd, uint16_t addr,
                              const uint8_t* data, uint8_t data_len) {
    // Trame : 5A A5 [len] [cmd] [addr_H] [addr_L] [data...]
    // len = nombre d'octets apres le champ longueur = cmd(1) + addr(2) + data
    uint8_t frame_len = 1 + 2 + data_len;  // cmd + addr + data
    uint8_t frame[64];
    frame[0] = DWIN_HEAD_H;
    frame[1] = DWIN_HEAD_L;
    frame[2] = frame_len;
    frame[3] = cmd;
    frame[4] = (uint8_t)(addr >> 8);
    frame[5] = (uint8_t)(addr & 0xFF);
    memcpy(&frame[6], data, data_len);
    _sendRaw(frame, 6 + data_len);
}

// ============================================================
//  _sendRaw()
// ============================================================
void DwinDisplay::_sendRaw(const uint8_t* buf, size_t len) {
    DWIN_SERIAL.write(buf, len);
}

// ============================================================
//  _readFrame() - Lit une trame depuis le DWIN (non-bloquant)
//  Retourne true si une trame complete a ete recue
// ============================================================
bool DwinDisplay::_readFrame() {
    while (DWIN_SERIAL.available()) {
        uint8_t b = DWIN_SERIAL.read();

        // Sync sur l'entete 5A A5
        if (_rxLen == 0 && b != DWIN_HEAD_H) continue;
        if (_rxLen == 1 && b != DWIN_HEAD_L) { _rxLen = 0; continue; }

        if (_rxLen < DWIN_RX_BUF_SIZE) {
            _rxBuf[_rxLen++] = b;
        }

        // Octet 2 = longueur du reste de la trame
        if (_rxLen >= 3) {
            uint8_t expected_total = 2 + _rxBuf[2] + 1;  // header(2) + len_byte(1) + payload
            if (_rxLen >= expected_total) {
                _parseFrame(_rxBuf, _rxLen);
                _rxLen = 0;
                return true;
            }
        }
    }
    return false;
}

// ============================================================
//  _parseFrame() - Decode la trame recue du DWIN
// ============================================================
void DwinDisplay::_parseFrame(const uint8_t* buf, uint8_t len) {
    if (len < 7) return;
    if (buf[0] != DWIN_HEAD_H || buf[1] != DWIN_HEAD_L) return;

    uint8_t cmd = buf[3];
    if (cmd != DWIN_CMD_READ_VP && cmd != 0x83) return;  // Seules les notifications VP

    uint16_t vp   = ((uint16_t)buf[4] << 8) | buf[5];
    uint8_t  words = buf[6];  // Nombre de mots de donnees

    if (len < (uint8_t)(7 + words * 2)) return;

    uint16_t value = ((uint16_t)buf[7] << 8) | buf[8];

    _lastEvent.vp_addr = vp;
    _lastEvent.value   = value;
    _lastEvent.valid   = true;

    Serial.printf("[DWIN] Touch VP=0x%04X val=%d\n", vp, value);

    if (_touchCb) {
        _touchCb(_lastEvent);
    }

    _lastEvent.valid = false;
}
