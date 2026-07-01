# 🚐 GG Van Dashboard

Tableau de bord tactile pour fourgon aménagé, basé sur un **écran Waveshare ESP32-S3-Touch-LCD-7B** (1024×600).
Il centralise énergie, batterie, solaire, chauffage, eau, GPS, météo et éclairage, et pilote une carte de relais distante via **ESP-NOW**.

---

## 🧩 Architecture

```
   ┌─────────────────────────────┐        ESP-NOW (sans fil)      ┌──────────────────────────┐
   │  MASTER — GG_Step22          │  ───────────────────────────► │  SLAVE — GG_Slave        │
   │  Waveshare ESP32-S3 7"       │  ◄─────────────────────────── │  ESP32 + carte relais    │
   │  Écran tactile + UI LVGL     │        (état / commandes)     │  Lumières, pompe, eau…   │
   └─────────────────────────────┘                               └──────────────────────────┘
          │  BLE          │ WiFi
          ▼               ▼
   Victron / JKBMS    Météo (OpenWeatherMap) + NTP
```

- **Master** (`Master_Console_Waveshare/GG_Step22/`) : l'écran tactile, l'interface, l'acquisition des données.
- **Slave** (`Slave_Carte_Relais/`) : carte à base d'ESP32 qui commande physiquement les relais (éclairages, pompe à eau, chauffe-eau) sur ordre du master via ESP-NOW.

---

## 🖥️ Le Master (GG_Step22) — ce qu'il fait

| Domaine | Détail |
|---|---|
| **Écran / UI** | Waveshare ESP32-S3-Touch-LCD-7B (1024×600, dalle RGB), tactile capacitif **GT911**, interface **LVGL 8.4** multi-pages (dashboard, énergie, chauffage, lumières, historique, système). |
| **☀️ Solaire (Victron)** | Lecture **BLE passive** (« Instant Readout ») des appareils Victron : MPPT SmartSolar, BMV-712, BatteryProtect, Orion/SmartShunt. Déchiffrement AES via *bindkey*. |
| **🔋 Batterie (JKBMS)** | Connexion **BLE GATT** au BMS JK (tension, courant, puissance, SOC, autonomie estimée). Se connecte uniquement quand on est sur la page Énergie. |
| **🔥 Chauffage** | Connexion **BLE GATT** au chauffage (Nordkapp) sur la page Chauffage. |
| **⚡ Relais / éclairage** | Commande la carte **Slave** via **ESP-NOW** (lumières salon/cuisine/chambre/WC/ext., pompe à eau, chauffe-eau). |
| **🌤️ Météo** | **OpenWeatherMap** (actuel + prévisions 5 jours) via WiFi, horloge **NTP**. |
| **🛰️ GPS** | Module NEO-6M (UART) — position, carte de France. |
| **🌡️ Capteurs** | BME280 (temp/hum/pression), SCD41 (CO₂), PIR (présence → mise en veille écran). |
| **💧 Eau** | Niveaux eau propre / eaux grises. |
| **💾 Historique** | Journalisation CSV sur carte SD (24 h glissantes + logs journaliers). |

---

## 💾 Carte SD — architecture

**La configuration et les ressources vivent sur la carte SD** (pas dans le code). Structure attendue à la racine de la SD :

```
/config.json          → configuration (WiFi, clés API, MAC/bindkeys BLE, réglages)
/icons/               → icônes de l'UI (BMP 64×64) : home, energy, heater, solar, wifi…
/images/              → images (ex. france_map.bmp)
/logs/                → journaux CSV par jour (AAAA-MM-JJ.csv)
/heater_*.csv         → historiques chauffage
```

### `config.json` — format (mettez VOS valeurs, pas celles d'exemple)

```json
{
  "wifi_ssid_1":           "VotreBox",
  "wifi_pass_1":           "votre_mot_de_passe",
  "wifi_ssid_2":           "AutreReseau",
  "wifi_pass_2":           "autre_mot_de_passe",
  "owm_api_key":           "votre_cle_openweathermap",
  "owm_city":              "Hirson,FR",
  "screen_dim_s":          90,
  "screen_off_s":          120,
  "ble_scan_s":            5,
  "jkbms_mac":             "XX:XX:XX:XX:XX:XX",
  "heating_mac":           "XX:XX:XX:XX:XX:XX",
  "victron_mppt_mac":      "XX:XX:XX:XX:XX:XX",
  "victron_mppt_bindkey":  "cle_aes_32_hex",
  "victron_bmv_mac":       "XX:XX:XX:XX:XX:XX",
  "victron_bmv_bindkey":   "cle_aes_32_hex",
  "victron_bp_mac":        "XX:XX:XX:XX:XX:XX",
  "victron_bp_bindkey":    "cle_aes_32_hex",
  "victron_orion_mac":     "XX:XX:XX:XX:XX:XX",
  "victron_orion_bindkey": "cle_aes_32_hex",
  "espnow_slave_mac":      "XX:XX:XX:XX:XX:XX"
}
```

> Le code charge ces champs au démarrage (`config_load()`) : les valeurs en dur dans les fichiers `.h`/`.cpp` ne sont que des **placeholders** — c'est la SD qui fait foi.
> **La MAC (bindkey Victron) se trouve dans VictronConnect → Appareil → Paramètres → Info produit.**

---

## 🔒 Sécurité

- **Aucun secret réel n'est versionné** : mots de passe WiFi, clé API météo, MAC et *bindkeys* Victron sont sur la **carte SD** (`config.json`), pas dans le dépôt.
- Les constantes dans le code sont des **placeholders** (`"wifi"`, `"mot_de_passe"`, `"00000…"`, `"XX:XX:…"`).

---

## 🔧 Compilation (recette validée)

- **Carte** : Waveshare ESP32-S3-Touch-LCD-7B (1024×600)
- **Core Arduino** : `esp32:esp32@3.3.10` (**IDF 5.5.4**)
- **FQBN** : `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc`
- **Bibliothèques** :
  - **LVGL 8.4** + `lv_conf.h` (⚠️ le *tick* LVGL est incrémenté manuellement dans `loop()` via `lv_tick_inc`, sinon le tactile ne répond pas)
  - **NimBLE-Arduino 2.5.0** (⚠️ doit correspondre à l'IDF du core — voir ci-dessous)
  - **ArduinoJson 6.21.x**
- **Drivers écran/tactile** fournis dans le projet (`rgb_lcd_port`, `gt911`, `touch`), pas de LovyanGFX.

### Points de calage importants (correctifs clés)
| Sujet | Réglage |
|---|---|
| 🖥️ Anti-scintillement RGB | **pixel clock 16 MHz** dans `rgb_lcd_port.h` (la PSRAM OPI tourne à 80 MHz sous Arduino) |
| 🖥️ Rendu LVGL | double framebuffer direct + `disp_drv.full_refresh = 1` |
| 👆 Tactile | `lv_tick_inc()` appelé dans `loop()` (`LV_TICK_CUSTOM=0`) |
| 🔵 BLE | **NimBLE 2.5.0** (aligné IDF 5.5) — une version décalée fait planter `esp_bt_controller_init` |

---

## 🔌 Le Slave (`Slave_Carte_Relais/GG_Slave_PCB`)

Carte à base d'**ESP32 classique** pilotant 8 relais/MOSFET + un chauffe-eau. Elle reçoit les
commandes du master via **ESP-NOW** (bitmask de relais), applique les sorties, et publie l'état en
**MQTT**. Protocole partagé `espnow_protocol.h` (identique au master).

### Mapping relais → GPIO

| Bit | Sortie | GPIO | Composant |
|----|--------|------|-----------|
| 0 | Salon | 26 | MOSFET Q13 |
| 1 | Cuisine | 27 | MOSFET Q12 |
| 2 | Chambre | 14 | MOSFET Q11 |
| 3 | WC | 12 | MOSFET Q10 |
| 4 | TV | 33 | MOSFET Q14 |
| 5 | Ext. Avant | 13 | MOSFET Q15 |
| 6 | Pompe à eau | 15 | MOSFET Q16 |
| 7 | Divers | 4 | MOSFET Q17 |
| — | Chauffe-eau | 25 | Relais K1 (champ dédié) |

### Compilation (recette validée)

- **Carte** : ESP32 Dev Module (ESP32 classique, **PAS** S3)
- **Core** : `esp32:esp32@3.3.10`
- **FQBN** : `esp32:esp32:esp32:PartitionScheme=default,FlashSize=4M,CPUFreq=240`
- **Bibliothèque** : PubSubClient (MQTT) — `esp_now` et `WiFi` sont dans le core
- ⚠️ **Callback ESP-NOW en signature core 3.x** : `onDataReceived(const esp_now_recv_info_t*, ...)`

> ⚠️ **Config WiFi / MQTT en dur** : contrairement au master (qui lit la SD), le slave a ses
> identifiants **dans le code** (`GG_Slave_PCB.ino`). Ce sont des **placeholders** dans ce dépôt —
> renseigne ton WiFi, ton broker MQTT et les MAC master/slave avant de compiler.

---

## 📁 Contenu du dépôt

```
GG_Dashboard/
├── Master_Console_Waveshare/   → MAÎTRE : dashboard tactile Waveshare 7"
│   └── GG_Step22/              → sketch Arduino
├── Slave_Carte_Relais/         → ESCLAVE : carte relais ESP-NOW
│   └── GG_Slave_PCB/           → sketch Arduino
└── README.md
```
