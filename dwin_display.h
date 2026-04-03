#pragma once
/*
 * dwin_display.h - Driver ecran DWIN DGUS2 (communication UART)
 *
 * Compatible avec les series :
 *   - DWIN T5L  (ex: DMG80480T070_A5W)
 *   - DWIN T5UIC1 (Creality Ender 3 V2, Ender 3 S1...)
 *   - DWIN DMG series (DMG48270C043, DMG80480C070...)
 *
 * Connexion UART :
 *   ESP32 TX2 (GPIO17) --> RX ecran DWIN
 *   ESP32 RX2 (GPIO16) --> TX ecran DWIN
 *   GND commun obligatoire
 *   Alimentation ecran : 5V ou 12V selon modele (NE PAS brancher sur 3.3V)
 *
 * Baud rate par defaut : 115200
 */
#include <Arduino.h>
#include <HardwareSerial.h>
#include "dwin_vp_map.h"

// ============================================================
//  Configuration UART
// ============================================================
#define DWIN_SERIAL       Serial2
#define DWIN_BAUD         115200
#define DWIN_RX_PIN       16
#define DWIN_TX_PIN       17
#define DWIN_RX_BUF_SIZE  64
#define DWIN_TX_TIMEOUT   50    // ms
#define DWIN_POLL_MS      100   // Intervalle de lecture des touches

// ============================================================
//  Structure des evenements recus du DWIN (touches tactiles)
// ============================================================
struct DwinTouchEvent {
    uint16_t vp_addr;    // Adresse VP de la touche
    uint16_t value;      // Valeur envoyee par le DWIN
    bool     valid;
};

// ============================================================
//  Classe principale DwinDisplay
// ============================================================
class DwinDisplay {
public:
    DwinDisplay();

    // Initialisation
    void begin();

    // Appeler dans loop() - gere la reception et les evenements
    void update();

    // ---- Navigation ----
    void showPage(uint8_t page_id);
    uint8_t currentPage() const { return _current_page; }

    // ---- Ecran general ----
    void setBacklight(uint8_t percent);  // 0-100
    void buzz(uint8_t ms = 50);

    // ---- Ecriture VP ----
    // Entiers
    void writeVP_u16(uint16_t vp, uint16_t value);
    void writeVP_i16(uint16_t vp, int16_t value);
    void writeVP_u32(uint16_t vp, uint32_t value);
    // Texte ASCII (taille max = len*2 octets, padde de 0xFF)
    void writeVP_text(uint16_t vp, const char* str, uint8_t max_chars = 16);
    // Icone (variable de type icone DWIN)
    void writeVP_icon(uint16_t vp, uint16_t icon_id);

    // ---- Raccourcis metier ----
    // Page Accueil
    void updateClock(uint8_t hour, uint8_t min, uint8_t sec);
    void updateDate(const char* date_str);
    void updateWifiStatus(bool connected, int8_t rssi_dbm, const char* ip);
    void updateBleStatus(uint8_t status);
    void updateWeather(uint8_t day, uint8_t icon_id, float temp_max,
                       float temp_min, uint8_t humidity);

    // Page Energie (JKBMS)
    void updateJkBms(float voltage, float current, float power,
                     float soc, float remain_ah, float temp_mos,
                     uint32_t cycles, bool balancing, uint8_t alarms);
    void updateCells(float cell_min, float cell_max, float cell_avg,
                     float delta_mv, uint8_t cell_count);

    // Page Energie (Victron MPPT)
    void updateMppt(uint8_t state, float pv_voltage, float pv_power,
                    float bat_voltage, float charge_current,
                    float charge_power, float yield_today, uint8_t error);

    // Page Energie (SmartShunt)
    void updateShunt(float voltage, float current, float power,
                     float soc, float ttg_min, float consumed_ah, bool alarm);

    // Page Eclairages
    void updateRelayStates(bool l1, bool l2, bool l3,
                           bool l4, bool l5, bool l6, bool tv);

    // Page Chauffage
    void updateHeating(bool on, float target_temp);

    // Page Systeme
    void updateSystemInfo(const char* ssid, const char* ip, const char* mac,
                          int8_t rssi, uint32_t uptime_s, uint16_t free_heap_kb,
                          float cpu_temp, const char* version, const char* chip_info,
                          bool jkbms_ok, bool mppt_ok, bool shunt_ok);

    // ---- Callback evenement tactile ----
    // A appeler pour enregistrer le handler de touches
    typedef void (*TouchCallback)(DwinTouchEvent evt);
    void setTouchCallback(TouchCallback cb) { _touchCb = cb; }

    // Dernier evenement recu (polling)
    DwinTouchEvent lastEvent() const { return _lastEvent; }

    static DwinDisplay* instance;

private:
    void _sendFrame(uint8_t cmd, uint16_t addr, const uint8_t* data, uint8_t len);
    void _sendRaw(const uint8_t* buf, size_t len);
    bool _readFrame();
    void _parseFrame(const uint8_t* buf, uint8_t len);
    uint8_t _owmIdToDwinIcon(int owm_id);

    uint8_t         _rxBuf[DWIN_RX_BUF_SIZE];
    uint8_t         _rxLen;
    uint32_t        _lastPollMs;
    uint8_t         _current_page;
    DwinTouchEvent  _lastEvent;
    TouchCallback   _touchCb;
};

extern DwinDisplay dwin;
