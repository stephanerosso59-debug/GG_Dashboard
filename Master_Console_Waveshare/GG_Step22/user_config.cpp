/*
 * user_config.cpp - Configuration utilisateur via JSON sur SD
 */
#include "user_config.h"
#include "sd_card.h"
#include "config_base.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SD_MMC.h"

// Variable globale
UserConfig userConfig;

#define CONFIG_PATH "/config.json"

// ────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────

// Helpers safe pour copier strings
static void safe_copy(char* dst, const char* src, size_t dst_size) {
    if (!src) {
        dst[0] = 0;
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = 0;
}

// Initialisation valeurs defaut (depuis defines compilees)
static void config_set_defaults() {
    // WiFi
    safe_copy(userConfig.wifi_ssid_1, "wifi", sizeof(userConfig.wifi_ssid_1));
    safe_copy(userConfig.wifi_pass_1, "mot_de_passe", sizeof(userConfig.wifi_pass_1));
    safe_copy(userConfig.wifi_ssid_2, "wifi", sizeof(userConfig.wifi_ssid_2));
    safe_copy(userConfig.wifi_pass_2, "mot_de_passe", sizeof(userConfig.wifi_pass_2));
    
    // OpenWeatherMap
    safe_copy(userConfig.owm_api_key, OWM_API_KEY, sizeof(userConfig.owm_api_key));
    safe_copy(userConfig.owm_city, OWM_CITY, sizeof(userConfig.owm_city));
    
    // Veille ecran
    userConfig.screen_dim_s = 90;
    userConfig.screen_off_s = 120;
    userConfig.ble_scan_s = 5;
    
    // BLE JKBMS
    safe_copy(userConfig.jkbms_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.jkbms_mac));
    
    // BLE Chauffage
    safe_copy(userConfig.heating_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.heating_mac));
    
    // BLE Victron MPPT
    safe_copy(userConfig.victron_mppt_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.victron_mppt_mac));
    safe_copy(userConfig.victron_mppt_bindkey, "00000000000000000000000000000000", sizeof(userConfig.victron_mppt_bindkey));
    
    // BLE Victron BMV
    safe_copy(userConfig.victron_bmv_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.victron_bmv_mac));
    safe_copy(userConfig.victron_bmv_bindkey, "00000000000000000000000000000000", sizeof(userConfig.victron_bmv_bindkey));
    
    // BLE Victron BatteryProtect
    safe_copy(userConfig.victron_bp_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.victron_bp_mac));
    safe_copy(userConfig.victron_bp_bindkey, "00000000000000000000000000000000", sizeof(userConfig.victron_bp_bindkey));
    
    // BLE Victron Orion DCDC
    safe_copy(userConfig.victron_orion_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.victron_orion_mac));
    safe_copy(userConfig.victron_orion_bindkey, "00000000000000000000000000000000", sizeof(userConfig.victron_orion_bindkey));
    
    // ESP-NOW slave
    safe_copy(userConfig.espnow_slave_mac, "XX:XX:XX:XX:XX:XX", sizeof(userConfig.espnow_slave_mac));
    
    userConfig.loaded_from_sd = false;
}

// Compteurs globaux pour les stats de fallback (utilises dans les helpers)
static int s_loaded_count = 0;
static int s_fallback_count = 0;
static int s_total_fields = 0;

// Helper : charge un champ string depuis JSON, retourne true si trouve
static bool load_field_str(JsonDocument& doc, const char* key, char* dst, size_t dst_size) {
    s_total_fields++;
    if (doc[key].is<const char*>()) {
        safe_copy(dst, doc[key], dst_size);
        Serial0.printf("  v %-22s = %s\n", key, dst);
        s_loaded_count++;
        return true;
    } else {
        Serial0.printf("  ! %-22s = (FALLBACK) %s\n", key, dst);
        s_fallback_count++;
        return false;
    }
}

// Helper : charge un champ int depuis JSON
static bool load_field_int(JsonDocument& doc, const char* key, uint32_t* dst) {
    s_total_fields++;
    if (doc[key].is<int>()) {
        *dst = doc[key];
        Serial0.printf("  v %-22s = %u\n", key, (unsigned)*dst);
        s_loaded_count++;
        return true;
    } else {
        Serial0.printf("  ! %-22s = (FALLBACK) %u\n", key, (unsigned)*dst);
        s_fallback_count++;
        return false;
    }
}

// ────────────────────────────────────────────────────────
// API publique
// ────────────────────────────────────────────────────────

bool config_load() {
    // Toujours commencer par les defauts (filet de securite)
    config_set_defaults();
    
    s_loaded_count = 0;
    s_fallback_count = 0;
    s_total_fields = 0;
    
    Serial0.println("==========================================");
    Serial0.println("[Config] Chargement configuration");
    Serial0.println("==========================================");
    
    if (!sdCard.mounted) {
        Serial0.println("[Config] SD non montee");
        Serial0.println("[Config] WARNING: TOUS les champs en FALLBACK (defauts compiles)");
        Serial0.println("[Config] -> Le dashboard fonctionne, mais aucune modification possible sans SD");
        Serial0.println("==========================================");
        return false;
    }
    
    if (!SD_MMC.exists(CONFIG_PATH)) {
        Serial0.println("[Config] /config.json absent sur SD");
        Serial0.println("[Config] -> Creation avec valeurs par defaut...");
        if (config_save()) {
            Serial0.println("[Config] /config.json cree avec succes");
            Serial0.println("[Config] -> Tu peux maintenant editer ce fichier sur ton PC");
        }
        Serial0.println("==========================================");
        return true;
    }
    
    File f = SD_MMC.open(CONFIG_PATH, FILE_READ);
    if (!f) {
        Serial0.println("[Config] Echec ouverture /config.json");
        Serial0.println("[Config] WARNING: TOUS les champs en FALLBACK");
        Serial0.println("==========================================");
        return false;
    }
    
    size_t file_size = f.size();
    Serial0.printf("[Config] /config.json trouve sur SD (%u octets)\n", (unsigned)file_size);
    
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    
    if (err) {
        Serial0.printf("[Config] ERREUR JSON: %s\n", err.c_str());
        Serial0.println("[Config] WARNING: TOUS les champs en FALLBACK");
        Serial0.println("[Config] -> Verifie ton config.json (syntaxe JSON cassee ?)");
        Serial0.println("==========================================");
        return false;
    }
    
    Serial0.println("[Config] Lecture JSON OK, parsing des champs :");
    Serial0.println("------------------------------------------");
    
    // === WiFi ===
    load_field_str(doc, "wifi_ssid_1", userConfig.wifi_ssid_1, sizeof(userConfig.wifi_ssid_1));
    load_field_str(doc, "wifi_pass_1", userConfig.wifi_pass_1, sizeof(userConfig.wifi_pass_1));
    load_field_str(doc, "wifi_ssid_2", userConfig.wifi_ssid_2, sizeof(userConfig.wifi_ssid_2));
    load_field_str(doc, "wifi_pass_2", userConfig.wifi_pass_2, sizeof(userConfig.wifi_pass_2));
    
    // === OpenWeatherMap ===
    load_field_str(doc, "owm_api_key", userConfig.owm_api_key, sizeof(userConfig.owm_api_key));
    load_field_str(doc, "owm_city", userConfig.owm_city, sizeof(userConfig.owm_city));
    
    // === Veille ecran ===
    load_field_int(doc, "screen_dim_s", &userConfig.screen_dim_s);
    load_field_int(doc, "screen_off_s", &userConfig.screen_off_s);
    load_field_int(doc, "ble_scan_s", &userConfig.ble_scan_s);
    
    // === BLE MAC + bindkeys ===
    load_field_str(doc, "jkbms_mac", userConfig.jkbms_mac, sizeof(userConfig.jkbms_mac));
    load_field_str(doc, "heating_mac", userConfig.heating_mac, sizeof(userConfig.heating_mac));
    load_field_str(doc, "victron_mppt_mac", userConfig.victron_mppt_mac, sizeof(userConfig.victron_mppt_mac));
    load_field_str(doc, "victron_mppt_bindkey", userConfig.victron_mppt_bindkey, sizeof(userConfig.victron_mppt_bindkey));
    load_field_str(doc, "victron_bmv_mac", userConfig.victron_bmv_mac, sizeof(userConfig.victron_bmv_mac));
    load_field_str(doc, "victron_bmv_bindkey", userConfig.victron_bmv_bindkey, sizeof(userConfig.victron_bmv_bindkey));
    load_field_str(doc, "victron_bp_mac", userConfig.victron_bp_mac, sizeof(userConfig.victron_bp_mac));
    load_field_str(doc, "victron_bp_bindkey", userConfig.victron_bp_bindkey, sizeof(userConfig.victron_bp_bindkey));
    load_field_str(doc, "victron_orion_mac", userConfig.victron_orion_mac, sizeof(userConfig.victron_orion_mac));
    load_field_str(doc, "victron_orion_bindkey", userConfig.victron_orion_bindkey, sizeof(userConfig.victron_orion_bindkey));
    load_field_str(doc, "espnow_slave_mac", userConfig.espnow_slave_mac, sizeof(userConfig.espnow_slave_mac));
    
    Serial0.println("------------------------------------------");
    Serial0.printf("[Config] %d/%d champs OK depuis SD\n", s_loaded_count, s_total_fields);
    
    if (s_fallback_count > 0) {
        Serial0.printf("[Config] WARNING: %d champs absents du JSON (FALLBACK utilise)\n", s_fallback_count);
        Serial0.println("[Config] -> Astuce : supprime /config.json sur SD pour le regenerer complet");
    } else {
        Serial0.println("[Config] Tous les champs charges depuis SD :)");
    }
    Serial0.println("==========================================");
    
    userConfig.loaded_from_sd = true;
    return true;
}

bool config_save() {
    if (!sdCard.mounted) {
        Serial0.println("[Config] SD non montee, sauvegarde impossible");
        return false;
    }
    
    StaticJsonDocument<2048> doc;
    
    // === WiFi ===
    doc["wifi_ssid_1"] = userConfig.wifi_ssid_1;
    doc["wifi_pass_1"] = userConfig.wifi_pass_1;
    doc["wifi_ssid_2"] = userConfig.wifi_ssid_2;
    doc["wifi_pass_2"] = userConfig.wifi_pass_2;
    
    // === OpenWeatherMap ===
    doc["owm_api_key"] = userConfig.owm_api_key;
    doc["owm_city"]    = userConfig.owm_city;
    
    // === Veille ecran ===
    doc["screen_dim_s"] = userConfig.screen_dim_s;
    doc["screen_off_s"] = userConfig.screen_off_s;
    doc["ble_scan_s"]   = userConfig.ble_scan_s;
    
    // === BLE Adresses MAC + bindkeys ===
    doc["jkbms_mac"]            = userConfig.jkbms_mac;
    doc["heating_mac"]          = userConfig.heating_mac;
    doc["victron_mppt_mac"]     = userConfig.victron_mppt_mac;
    doc["victron_mppt_bindkey"] = userConfig.victron_mppt_bindkey;
    doc["victron_bmv_mac"]      = userConfig.victron_bmv_mac;
    doc["victron_bmv_bindkey"]  = userConfig.victron_bmv_bindkey;
    doc["victron_bp_mac"]       = userConfig.victron_bp_mac;
    doc["victron_bp_bindkey"]   = userConfig.victron_bp_bindkey;
    doc["victron_orion_mac"]    = userConfig.victron_orion_mac;
    doc["victron_orion_bindkey"] = userConfig.victron_orion_bindkey;
    
    // === ESP-NOW ===
    doc["espnow_slave_mac"]     = userConfig.espnow_slave_mac;
    
    File f = SD_MMC.open(CONFIG_PATH, FILE_WRITE);
    if (!f) {
        Serial0.println("[Config] Echec ouverture config.json en ecriture");
        return false;
    }
    
    serializeJsonPretty(doc, f);
    f.close();
    
    Serial0.printf("[Config] Sauvegarde dans %s\n", CONFIG_PATH);
    return true;
}
