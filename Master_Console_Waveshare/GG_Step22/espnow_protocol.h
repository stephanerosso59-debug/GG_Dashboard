#pragma once
/*
 * espnow_protocol.h - Protocole ESP-NOW partage entre :
 *   - Master  : Waveshare ESP32-S3 (dashboard)
 *   - Slave   : ESP32-U sur PCB (relais + ADC eau)
 * 
 * Ce fichier est INCLUS DES DEUX COTES — ne pas modifier l'un sans l'autre !
 *
 * Architecture :
 *   - Master envoie des commandes relais (CmdRelays)
 *   - Slave envoie son etat (StatusUpdate) a 1Hz et a chaque changement
 *   - Channel WiFi : 6 (par defaut)
 *   - Encryption : aucune (LAN local, pas critique)
 */
#include <Arduino.h>
#include <stdint.h>

// Channel WiFi par defaut
#define ESPNOW_CHANNEL 6

// MAC des deux peers
// Master : Waveshare ESP32-S3  → 3C:0F:02:C9:DE:A0
// Slave  : ESP32-U PCB         → XX:XX:XX:XX:XX:XX
extern uint8_t MASTER_MAC[6];
extern uint8_t SLAVE_MAC[6];

// Types de message
#define MSG_CMD_RELAYS    0x01   // Master -> Slave : commande relais
#define MSG_STATUS        0x02   // Slave -> Master : etat capteurs
#define MSG_PING          0x03   // Master -> Slave : keepalive
#define MSG_PONG          0x04   // Slave -> Master : reponse ping

// ═══════════════════════════════════════════════════════════
//  Master -> Slave : commandes relais
//  Taille : 12 bytes (inchangée)
// ═══════════════════════════════════════════════════════════
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;        // = MSG_CMD_RELAYS
    uint8_t  relays_state;    // bitmask 8 bits (relais 1..8 = bit 0..7)
                              //   bit 0 = LIGHT1 Salon   (GPIO26)
                              //   bit 1 = LIGHT2 Cuisine (GPIO27)
                              //   bit 2 = LIGHT3 Chambre (GPIO14)
                              //   bit 3 = LIGHT4 WC      (GPIO12)
                              //   bit 4 = TV             (GPIO33)
                              //   bit 5 = LIGHT5 Ext Av. (GPIO13)
                              //   bit 6 = WATER_PUMP     (GPIO15)
                              //   bit 7 = DIVERS         (GPIO4)
    uint8_t  chauffe_eau;     // 0=OFF, 1=ON → MMBT2222A K1 (GPIO25)
    uint8_t  reserved[5];     // réservé future utilisation
    uint32_t timestamp_ms;    // millis() emetteur
} CmdRelaysMsg;

// ═══════════════════════════════════════════════════════════
//  Slave -> Master : etat
// ═══════════════════════════════════════════════════════════
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;        // = MSG_STATUS

    // Niveaux eau (CBE Solid State sondes capacitives)
    float    water_clean_pct;  // 0-100% niveau eau propre (GPIO 34)
    float    water_dirty_pct;  // 0-100% niveau eau sale  (GPIO 35)

    // Etat reel relais (feedback)
    uint8_t  relays_actual;    // bitmask (meme mapping que relays_state)
    uint8_t  chauffe_eau_actual; // 0=OFF 1=ON

    // Telemetry slave
    float    voltage_in;       // tension entree PCB 12V (via diviseur)
    int16_t  rssi;             // RSSI dernier message ESP-NOW recu
    uint32_t uptime_s;         // secondes depuis boot du slave
    uint8_t  reserved[5];
    uint32_t timestamp_ms;
} StatusMsg;

// ═══════════════════════════════════════════════════════════
//  Master <-> Slave : ping / pong
// ═══════════════════════════════════════════════════════════
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;        // = MSG_PING ou MSG_PONG
    uint32_t timestamp_ms;
} PingMsg;

// ═══════════════════════════════════════════════════════════
//  Macros utilitaires bitmask relais
// ═══════════════════════════════════════════════════════════
#define RELAY_BIT_SALON    (1 << 0)
#define RELAY_BIT_CUISINE  (1 << 1)
#define RELAY_BIT_CHAMBRE  (1 << 2)
#define RELAY_BIT_WC       (1 << 3)
#define RELAY_BIT_TV       (1 << 4)
#define RELAY_BIT_EXT_AV   (1 << 5)
#define RELAY_BIT_PUMP     (1 << 6)
#define RELAY_BIT_DIVERS   (1 << 7)
