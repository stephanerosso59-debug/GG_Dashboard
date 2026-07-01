/*
 * espnow_master.cpp - Cote Waveshare (master)
 */
#include "espnow_master.h"
#include "user_config.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <string.h>

// ─── MAC ESP32-U a remplir une fois connue ───
// Pour la trouver : flasher le slave, regarder son Serial0 au boot
uint8_t MASTER_MAC[6] = {0,0,0,0,0,0};        // sera lu au boot
uint8_t SLAVE_MAC[6]  = {0x00,0x00,0x00,0x00,0x00,0x00};  // Nouvelle ESP32 30pins PCB

// Données reçues
StatusMsg statusFromSlave = {};
bool      statusValid = false;
uint32_t  statusLastUpdateMs = 0;
bool      slaveOnline = false;

// ─── Callbacks ───
static void on_data_recv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < 1) return;
    
    uint8_t msg_type = data[0];
    
    switch (msg_type) {
        case MSG_STATUS:
            if (len >= (int)sizeof(StatusMsg)) {
                memcpy(&statusFromSlave, data, sizeof(StatusMsg));
                statusValid = true;
                statusLastUpdateMs = millis();
                slaveOnline = true;
            }
            break;
        case MSG_PONG:
            slaveOnline = true;
            break;
        default:
            Serial0.printf("[ESPNOW] Message type inconnu 0x%02X\n", msg_type);
            break;
    }
}

// Compatibilite multi-versions ESP32 Core :
//   - Core <= 3.2.x (Waveshare XIP 3.1.1) : signature ancienne const uint8_t*
//   - Core >= 3.3.x                       : signature nouvelle wifi_tx_info_t*
#include <core_version.h>
#if defined(ESP_ARDUINO_VERSION) && defined(ESP_ARDUINO_VERSION_VAL) && \
    ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,3,0)
  #define ESPNOW_USE_NEW_API 1
#else
  #define ESPNOW_USE_NEW_API 0
#endif

#if ESPNOW_USE_NEW_API
static void on_data_sent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        static uint32_t last_warn = 0;
        if (millis() - last_warn > 10000) {
            const uint8_t* mac = (info && info->des_addr) ? info->des_addr : nullptr;
            if (mac) {
                Serial0.printf("[ESPNOW] Send fail vers %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
            } else {
                Serial0.println("[ESPNOW] Send fail (mac inconnue)");
            }
            last_warn = millis();
        }
    }
}
#else
static void on_data_sent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        static uint32_t last_warn = 0;
        if (millis() - last_warn > 10000) {
            if (mac) {
                Serial0.printf("[ESPNOW] Send fail vers %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
            } else {
                Serial0.println("[ESPNOW] Send fail (mac inconnue)");
            }
            last_warn = millis();
        }
    }
}
#endif

// Parse une chaine MAC "XX:XX:XX:XX:XX:XX" vers tableau de 6 bytes
static bool parse_mac_string(const char* s, uint8_t out[6]) {
    if (!s) return false;
    int v[6];
    int n = sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]);
    if (n != 6) return false;
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
    return true;
}

void espnow_master_init() {
    // Charger SLAVE_MAC depuis userConfig (par defaut hardcoded sinon)
    uint8_t parsed[6];
    if (parse_mac_string(userConfig.espnow_slave_mac, parsed)) {
        memcpy(SLAVE_MAC, parsed, 6);
        Serial0.printf("[ESPNOW] Slave MAC (depuis config): %s\n", userConfig.espnow_slave_mac);
    }
    
    // Recuperer MAC locale
    WiFi.macAddress(MASTER_MAC);
    Serial0.printf("[ESPNOW] Master MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        MASTER_MAC[0],MASTER_MAC[1],MASTER_MAC[2],
        MASTER_MAC[3],MASTER_MAC[4],MASTER_MAC[5]);
    
    // Le WiFi doit etre en mode station (deja le cas grace au WiFiMulti)
    if (esp_now_init() != ESP_OK) {
        Serial0.println("[ESPNOW] esp_now_init FAIL");
        return;
    }
    
    // Detecter le canal WiFi actuel (impose par l'AP si WiFi connecte)
    uint8_t wifi_channel = ESPNOW_CHANNEL;
    wifi_second_chan_t second_chan;
    if (WiFi.isConnected()) {
        esp_wifi_get_channel(&wifi_channel, &second_chan);
        Serial0.printf("[ESPNOW] *** WiFi connecte sur canal %d ***\n", wifi_channel);
        Serial0.printf("[ESPNOW] *** REPORTE CE CANAL DANS LE SLAVE PCB ***\n");
    } else {
        // Pas de WiFi -> on peut forcer notre canal
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        Serial0.printf("[ESPNOW] Canal force a %d (pas de WiFi)\n", ESPNOW_CHANNEL);
    }
    
    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);
    
    // Ajouter le slave comme peer SUR LE BON CANAL
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, SLAVE_MAC, 6);
    peer.channel = wifi_channel;   // canal aligne avec WiFi
    peer.encrypt = false;
    peer.ifidx   = WIFI_IF_STA;
    
    esp_err_t res = esp_now_add_peer(&peer);
    if (res != ESP_OK) {
        Serial0.printf("[ESPNOW] add_peer fail: %d\n", res);
    } else {
        Serial0.printf("[ESPNOW] Master initialise sur canal %d, peer slave ajoute\n", wifi_channel);
    }
}

bool espnow_send_relays(uint8_t bitmask, uint8_t chauffe_eau) {
    CmdRelaysMsg msg = {};
    msg.msg_type     = MSG_CMD_RELAYS;
    msg.relays_state = bitmask;
    msg.chauffe_eau  = chauffe_eau;
    msg.timestamp_ms = millis();
    
    esp_err_t res = esp_now_send(SLAVE_MAC, (uint8_t*)&msg, sizeof(msg));
    if (res != ESP_OK) {
        Serial0.printf("[ESPNOW] send_relays fail: %d\n", res);
        return false;
    }
    Serial0.printf("[ESPNOW] -> Relais 0x%02X envoye\n", bitmask);
    return true;
}

void espnow_send_ping() {
    PingMsg msg = {};
    msg.msg_type = MSG_PING;
    msg.timestamp_ms = millis();
    esp_now_send(SLAVE_MAC, (uint8_t*)&msg, sizeof(msg));
}
