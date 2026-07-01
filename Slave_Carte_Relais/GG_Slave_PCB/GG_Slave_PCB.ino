// ============================================
// GG_Slave_PCB.ino - Van GG Slave ESP32
// Contrôle relais/MOSFET via ESP-NOW
// depuis Master Waveshare GG_Step22
// ============================================
// MAC Slave  : XX:XX:XX:XX:XX:XX
// MAC Master : XX:XX:XX:XX:XX:XX
// MQTT Broker: 192.168.1.XXX:1883 (vangg/mot_de_passe)
// ============================================

#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "espnow_protocol.h"

// ============================================
// GPIO MAPPING
// ============================================
#define RELAY_LIGHT1_SALON   26   // MOSFET Q13 — bit 0
#define RELAY_LIGHT2_CUISINE 27   // MOSFET Q12 — bit 1
#define RELAY_LIGHT3_CHAMBRE 14   // MOSFET Q11 — bit 2
#define RELAY_LIGHT4_WC      12   // MOSFET Q10 — bit 3
#define RELAY_TV             33   // MOSFET Q14 — bit 4
#define RELAY_LIGHT5_EXT_AV  13   // MOSFET Q15 — bit 5
#define RELAY_WATER_PUMP     15   // MOSFET Q16 — bit 6
#define RELAY_DIVERS         4    // MOSFET Q17 — bit 7
#define RELAY_CHAUFFE_EAU    25   // MMBT2222A K1 (champ dédié)

// ============================================
// WIFI / MQTT
// ============================================
const char* WIFI_SSID     = "VotreWiFi";
const char* WIFI_PASS     = "mot_de_passe";
const char* MQTT_SERVER   = "192.168.1.XXX";
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "utilisateur_mqtt";
const char* MQTT_PASS     = "mot_de_passe";
const char* MQTT_CLIENT   = "GG_Slave_PCB";

#define TOPIC_STATUS   "vangg/slave/status"
#define TOPIC_CMD      "vangg/slave/cmd"
#define TOPIC_RELAYS   "vangg/slave/relays"

// MAC du master
uint8_t MASTER_MAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t SLAVE_MAC[]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Etat courant
uint8_t currentRelays    = 0;
uint8_t currentChauffeEau = 0;

// GPIO par bit
const int RELAY_PINS[8] = {
    RELAY_LIGHT1_SALON,
    RELAY_LIGHT2_CUISINE,
    RELAY_LIGHT3_CHAMBRE,
    RELAY_LIGHT4_WC,
    RELAY_TV,
    RELAY_LIGHT5_EXT_AV,
    RELAY_WATER_PUMP,
    RELAY_DIVERS
};

const char* RELAY_NAMES[8] = {
    "Salon(26)", "Cuisine(27)", "Chambre(14)", "WC(12)",
    "TV(33)", "ExtAv(13)", "Pompe(15)", "Divers(4)"
};

// ============================================
// INIT GPIO
// ============================================
void initGPIO() {
    for (int i = 0; i < 8; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
        Serial.printf("GPIO %s → OUTPUT LOW\n", RELAY_NAMES[i]);
    }
    pinMode(RELAY_CHAUFFE_EAU, OUTPUT);
    digitalWrite(RELAY_CHAUFFE_EAU, LOW);
    Serial.println("GPIO ChauffeEau(25) → OUTPUT LOW");
}

// ============================================
// APPLIQUER LES RELAIS via bitmask
// ============================================
void applyRelays(uint8_t bitmask, uint8_t chauffe_eau) {
    for (int i = 0; i < 8; i++) {
        bool state = (bitmask >> i) & 0x01;
        digitalWrite(RELAY_PINS[i], state ? HIGH : LOW);
    }
    digitalWrite(RELAY_CHAUFFE_EAU, chauffe_eau ? HIGH : LOW);
    currentRelays     = bitmask;
    currentChauffeEau = chauffe_eau;

    Serial.printf("[Relais] bitmask=0x%02X CE=%d\n", bitmask, chauffe_eau);
    Serial.printf("  Salon=%d Cuisine=%d Chambre=%d WC=%d TV=%d ExtAv=%d Pompe=%d Divers=%d\n",
        (bitmask>>0)&1, (bitmask>>1)&1, (bitmask>>2)&1, (bitmask>>3)&1,
        (bitmask>>4)&1, (bitmask>>5)&1, (bitmask>>6)&1, (bitmask>>7)&1);
}

// ============================================
// CALLBACK ESP-NOW
// ============================================
void onDataReceived(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < 1) return;
    uint8_t msgType = data[0];

    if (msgType == MSG_CMD_RELAYS && len >= (int)sizeof(CmdRelaysMsg)) {
        CmdRelaysMsg* cmd = (CmdRelaysMsg*)data;
        applyRelays(cmd->relays_state, cmd->chauffe_eau);
        publishRelayStatus();
    }
    else if (msgType == MSG_PING && len >= (int)sizeof(PingMsg)) {
        PingMsg pong;
        pong.msg_type    = MSG_PONG;
        pong.timestamp_ms = millis();
        esp_now_send(MASTER_MAC, (uint8_t*)&pong, sizeof(pong));
    }
}

// ============================================
// MQTT
// ============================================
void publishRelayStatus() {
    if (!mqttClient.connected()) return;
    char json[256];
    snprintf(json, sizeof(json),
        "{\"salon\":%d,\"cuisine\":%d,\"chambre\":%d,\"wc\":%d,"
        "\"tv\":%d,\"ext_av\":%d,\"pompe\":%d,\"divers\":%d,\"chauffe_eau\":%d}",
        (currentRelays>>0)&1, (currentRelays>>1)&1,
        (currentRelays>>2)&1, (currentRelays>>3)&1,
        (currentRelays>>4)&1, (currentRelays>>5)&1,
        (currentRelays>>6)&1, (currentRelays>>7)&1,
        currentChauffeEau);
    mqttClient.publish(TOPIC_RELAYS, json, true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Commandes MQTT si besoin
}

void reconnectMQTT() {
    int tries = 0;
    while (!mqttClient.connected() && tries < 3) {
        Serial.print("[MQTT] Connexion...");
        if (mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
            Serial.println(" OK");
            mqttClient.subscribe(TOPIC_CMD);
            mqttClient.publish(TOPIC_STATUS, "{\"status\":\"online\"}", true);
        } else {
            Serial.printf(" Echec rc=%d\n", mqttClient.state());
            delay(2000);
        }
        tries++;
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== GG_Slave_PCB démarrage ===");

    initGPIO();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int wifiTries = 0;
    while (WiFi.status() != WL_CONNECTED && wifiTries < 20) {
        delay(500);
        Serial.print(".");
        wifiTries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connecté: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WiFi] Echec — ESP-NOW only mode");
    }

    Serial.printf("MAC Slave: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init ERREUR");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MASTER_MAC, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    Serial.println("[ESP-NOW] Init OK");

    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    reconnectMQTT();

    Serial.println("=== GG_Slave_PCB prêt ===");
}

// ============================================
// LOOP
// ============================================
void loop() {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();
    delay(10);
}
