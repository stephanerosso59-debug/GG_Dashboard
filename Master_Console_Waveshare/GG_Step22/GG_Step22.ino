/*
 * GG Van Dashboard - Step 3
 * Navigation 5 pages + WiFi Multi (maison + van + camping)
 */
#include "rgb_lcd_port.h"
#include "gui_paint.h"
#include "image.h"
#include "user_wifi.h"
#include "gt911.h"
#include "i2c.h"
#include "io_extension.h"
#include "sd_card.h"
#include "sd_lvgl_fs.h"
#include "icons_sd.h"
#include "theme_mode.h"
#include "user_config.h"
#include <time.h>
#include <lvgl.h>
#include "lv_conf.h"
#include "config.h"
#include "ui.h"
#include "bme_wire1.h"
#include "scd41.h"
#include "pir_sensor.h"
#include "screen_sleep.h"
#include "history.h"
#include "weather.h"
#include "ble_task.h"
#include "water_state.h"
#include "gps.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include "espnow_master.h"

WiFiMulti wifiMulti;

// ── Reseaux WiFi ─────────────────────────────────────────────
//  La VRAIE config (SSID + mots de passe) est chargee depuis la carte SD
//  (config.json) via config_load() -> userConfig.wifi_ssid_x / wifi_pass_x.
//  Ces #define ne sont que des PLACEHOLDERS non-secrets : NE PAS mettre de
//  vrais mots de passe ici, le depot GitHub est public.
#define WIFI_SSID_1   "wifi"          // placeholder -> vrai SSID dans config.json (SD)
#define WIFI_PASS_1   "mot_de_passe"  // placeholder -> vrai mdp  dans config.json (SD)
#define WIFI_SSID_2   "wifi"          // placeholder
#define WIFI_PASS_2   "mot_de_passe"  // placeholder

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    // full_refresh=1 : color_map EST l'un des 2 framebuffers du panneau RGB.
    // esp_lcd_panel_draw_bitmap detecte le pointeur -> simple swap, zero recopie.
    wavesahre_rgb_lcd_display((uint8_t *)color_map);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    touch_gt911_point_t point = touch_gt911_read_point(1);
    if (point.cnt > 0) {
        data->point.x = point.x[0];
        data->point.y = point.y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        // Tout touch reset le timer d'inactivité
        display_force_on();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t t_update    = 0;
static uint32_t t_wifi_check = 0;
static uint32_t t_espnow_ping = 0;

void setup() {
    Serial0.begin(115200);
    // Attendre que le port USB-CDC soit prêt (max 5s)
    uint32_t t0 = millis();
    while (!Serial0 && (millis() - t0) < 5000) {
        delay(50);
    }
    delay(500);
    Serial0.println();
    Serial0.println("========================================");
    Serial0.println("   GG Van Dashboard - Step 22 boot");
    Serial0.println("========================================");
    Serial0.flush();
    if (psramFound()) Serial0.println("[Boot] PSRAM OK");
    Serial0.flush();

    DEV_I2C_Init();
    Serial0.println("[Boot] I2C init OK");
    Serial0.flush();

    IO_EXTENSION_Init();
    Serial0.println("[Boot] IO extension OK");
    
    // ── Carte SD (juste apres IO_EXTENSION car la SD utilise EXIO4) ──
    if (sd_card_init()) {
        Serial0.println("[Boot] SD card OK");
        sd_card_list_root();
    } else {
        Serial0.println("[Boot] SD card absente ou erreur (le dashboard fonctionne sans)");
    }
    Serial0.flush();
    
    // ── Charger configuration utilisateur depuis SD ──
    config_load();
    Serial0.flush();

    waveshare_esp32_s3_rgb_lcd_init();
    Serial0.println("[Boot] RGB LCD OK");
    Serial0.flush();

    wavesahre_rgb_lcd_bl_on();
    touch_gt911_init();
    Serial0.println("[Boot] Touch GT911 OK");
    Serial0.flush();

    lv_init();

    // ── LVGL en double buffer DIRECT sur les 2 framebuffers du panneau RGB ──
    // full_refresh=1 : LVGL rend l'image entiere dans un fb, le flush ne fait
    // qu'un swap (zero recopie) -> plus de scintillement, boucle fluide.
    void *fb1 = NULL, *fb2 = NULL;
    waveshare_get_frame_buffer(&fb1, &fb2);
    size_t fb_bytes = (size_t)EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;
    if (fb1) memset(fb1, 0, fb_bytes);
    if (fb2) memset(fb2, 0, fb_bytes);
    Serial0.printf("[Boot] Framebuffers RGB: fb1=%p fb2=%p\n", fb1, fb2);
    Serial0.flush();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, fb1, fb2,
                          EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res      = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb     = lvgl_flush_cb;
    disp_drv.draw_buf     = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);
    Serial0.println("[Boot] LVGL OK");
    
    // Driver LVGL filesystem pour SD card (lettre 'S:')
    sd_lvgl_fs_init();
    
    // Charger les icones depuis la SD (best effort)
    icons_sd_load_all();

    ui.begin();
    Serial0.println("[Boot] UI OK");

    // BME280 sur Wire1 (GPIO 21/22)
    bme_wire1_init();
    Serial0.println("[Boot] BME280 OK");

    // SCD41 (CO2) sur bus I2C partagé
    scd41_init();
    Serial0.println("[Boot] SCD41 OK");

    // PIR sur GPIO 6
    pir_init();
    Serial0.println("[Boot] PIR OK");
    
    screen_sleep_init();
    Serial0.println("[Boot] Screen sleep manager OK");
    
    history_init();
    Serial0.println("[Boot] History buffers OK");
    Serial0.println("[Boot] PIR OK");

    // Eau (CBE placeholder - sera remplace par ESP-NOW)
    water_init();
    Serial0.println("[Boot] Water init OK");
    
    // GPS NEO-6M
    gps_init();
    Serial0.println("[Boot] GPS init OK");

    // ── BLE EN PREMIER (fix 2026-07-01) ──────────────────────
    // Le controleur BT (esp_bt_controller_init) a besoin d'un gros bloc
    // de RAM interne contigu. Si WiFi demarre avant, il ne reste plus
    // assez -> crash LoadProhibited. On lance donc BLE d'abord et on
    // laisse son init se terminer avant de demarrer le WiFi.
    // ⚠️ 2026-07-01 : BLE désactivé — NimBLE 1.4.3 incompatible core 3.3.10/IDF5.1
    //     (crash confirmé, cf. issue h2zero/NimBLE-Arduino #641).
    //     Réactivé le 2026-07-01 après migration NimBLE 1.4.3 -> 2.3.4.
    Serial0.println("[Boot] Demarrage tache BLE sur Core 0 (AVANT WiFi)...");
    ble_task_start();
    delay(2500);

    // ── WiFi Multi-réseaux ──────────────────────────────────
    WiFi.mode(WIFI_STA);
    delay(100);
    wifiMulti.addAP(userConfig.wifi_ssid_1, userConfig.wifi_pass_1);
    wifiMulti.addAP(userConfig.wifi_ssid_2, userConfig.wifi_pass_2);
    Serial0.println("[Boot] Scan WiFi en cours (max 15s)...");
    
    // Tentative connexion avec timeout 15s
    uint32_t wifi_start = millis();
    uint8_t wifi_status = WL_DISCONNECTED;
    while ((millis() - wifi_start) < 15000) {
        wifi_status = wifiMulti.run(5000);  // timeout 5s par essai
        if (wifi_status == WL_CONNECTED) break;
        Serial0.printf("[Boot] WiFi tentative... status=%d (5s)\n", wifi_status);
        delay(500);
    }
    
    if (wifi_status == WL_CONNECTED) {
        Serial0.printf("[Boot] WiFi OK: %s (IP %s) RSSI=%d dBm\n",
            WiFi.SSID().c_str(),
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI());
        // NTP : UTC+1 (Paris hiver) + DST géré auto
        configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
        Serial0.println("[Boot] NTP configure");
    } else {
        Serial0.println("[Boot] *** WiFi NON connecte (verifie SSID/passwords) ***");
        Serial0.println("[Boot] Le dashboard fonctionnera sans WiFi");
        // Lister ce qu'on voit pour aider le diagnostic
        Serial0.println("[Boot] Reseaux WiFi visibles autour:");
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n && i < 8; i++) {
            Serial0.printf("  - %s (RSSI %d dBm) %s\n",
                WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "[OPEN]" : "[secured]");
        }
    }

    // ── ESP-NOW (master) ─────────────────────────────────────
    Serial0.println("[Boot] Init ESP-NOW...");
    espnow_master_init();

    // (BLE deja demarre plus haut, avant le WiFi)

    Serial0.println("[Boot] PRET!");
    Serial0.printf("[Boot] Free heap: %u KB, PSRAM: %u KB\n",
        ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
}

static uint32_t t_sensors_ms = 0;
static uint32_t t_ui_update_ms = 0;

void loop() {
    uint32_t now = millis();

    // ── Tick LVGL : LV_TICK_CUSTOM=0 dans lv_conf.h -> LVGL n'avance pas le
    //    temps tout seul. Sans ca, la lecture du tactile (indev read period,
    //    basee sur le temps) n'est JAMAIS declenchee -> ecran OK mais tactile mort.
    static uint32_t lv_last_tick = 0;
    if (lv_last_tick == 0) lv_last_tick = now;
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;

    // ══ PRIORITE 1 : LVGL + touch (le plus souvent possible) ══
    lv_timer_handler();

    // ══ PRIORITE 2 : capteurs lents (toutes 500ms) ══
    if (now - t_sensors_ms >= 500) {
        t_sensors_ms = now;
        bme_wire1_update();
        scd41_update();
        weather_update();
        weather_update_7days();
        water_update();
        
        // Une seule fois apres synchro NTP : recharger l'historique du jour depuis SD
        static bool history_csv_loaded = false;
        if (!history_csv_loaded && time(nullptr) > 8 * 3600) {
            history_load_from_sd();
            history_csv_loaded = true;
        }

        // Reconnexion WiFi auto
        if (now - t_wifi_check >= 30000) {
            t_wifi_check = now;
            wifiMulti.run();
        }
        
        // Ping ESP-NOW toutes les 5s pour que le slave sache qu'on est vivant
        if (now - t_espnow_ping >= 5000) {
            t_espnow_ping = now;
            espnow_send_ping();
        }
    }

    // ══ PRIORITE 3 : capteurs rapides (chaque iteration) ══
    pir_update();
    gps_update();
    screen_sleep_update();
    history_update();   // record toutes les 10 min
    
    // ══ Mode jour/nuit (check toutes les 30s) ══
    static uint32_t t_theme_ms = 0;
    if (now - t_theme_ms >= 30000) {
        t_theme_ms = now;
        theme_update_auto();
    }

    // ══ PRIORITE 4 : UI refresh valeurs (1 fois par seconde) ══
    if (now - t_ui_update_ms >= 1000) {
        t_ui_update_ms = now;
        ui.update();
    }

    delay(2);
}
