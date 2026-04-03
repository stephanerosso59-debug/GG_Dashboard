# workspace_lvgl — ESP32 + TFT ILI9341 + LVGL

Workspace complet pour l'affichage van sur ecran TFT SPI (ILI9341 480x320)
avec animations LVGL et communication BLE (JKBMS + Victron).

## Prerequis

- PlatformIO (VS Code extension ou CLI)
- ESP32 (WROOM-32 ou WROVER)
- Ecran TFT ILI9341 480x320 avec tactile XPT2046

## Pinout ILI9341

| Ecran      | ESP32 GPIO |
|------------|-----------|
| MISO       | 19        |
| MOSI       | 23        |
| CLK / SCK  | 18        |
| CS         | 5         |
| DC / RS    | 2         |
| RST        | 4         |
| TOUCH CS   | 15        |
| VCC        | 3.3V      |
| GND        | GND       |

## Pinout relais (actif LOW)

| Canal      | GPIO |
|------------|------|
| Lumiere 1  | 26   |
| Lumiere 2  | 27   |
| Lumiere 3  | 14   |
| Lumiere 4  | 12   |
| Lumiere 5  | 13   |
| Lumiere 6  | 25   |
| Television | 33   |
| Chauffage  | 32   |

## Configuration

1. Editer `../shared/config_base.h` :
   - `WIFI_SSID` / `WIFI_PASSWORD`
   - `JKBMS_MAC`, `VICTRON_MPPT_MAC`, `VICTRON_SHUNT_MAC`
   - Cles BLE Victron (`VICTRON_MPPT_KEY`, `VICTRON_SHUNT_KEY`)
   - `OWM_API_KEY` (gratuit sur openweathermap.org)

## Compilation et flash

```bash
# Compiler
pio run -e esp32_lvgl

# Compiler + flasher
pio run -e esp32_lvgl --target upload

# Monitor serie
pio device monitor
```

## Librairies utilisees

| Librairie         | Version | Role                        |
|-------------------|---------|-----------------------------|
| NimBLE-Arduino    | ^1.4.2  | BLE JKBMS + Victron         |
| lvgl              | ^8.3.11 | Interface graphique         |
| TFT_eSPI          | ^2.5.43 | Driver ILI9341 SPI          |
| ArduinoJson       | ^6.21.5 | Parsing JSON meteo          |
| NTPClient         | ^3.2.1  | Heure NTP                   |

## Structure

```
Dasboard GG/
├── platformio.ini
├── lib/
│   └── lv_conf.h          Configuration LVGL
└── src/
    ├── main.cpp            Point d'entree
    ├── config.h            Pins TFT + periodes
    └── ui/
        ├── ui.cpp/.h       Gestionnaire pages LVGL
        ├── page_home.cpp   Page accueil + van silhouette
        ├── page_lights.cpp Controle 6 lumieres + TV
        ├── page_battery.cpp Energie JKBMS + Victron
        ├── page_heating.cpp Chauffage avec consigne
        ├── lvgl_anim_icons.h  Moteur animation lv_timer
        ├── lvgl_icons_data.h  Frames RGB565 (genere)
        └── van_ui_anim_init.cpp Enregistrement animations


Code BLE et meteo partage avec les autres Dasboard GG dans `
../shared/`
│   ├── ble/ 
		├── ble_scanner.cpp
        ├── ble_scanner.h   
        ├── heating_ble.cpp
		├── heating_ble.h
		├── jkbms_ble.cpp
		├── jkbms_ble.h
		├── victron_ble.cpp
		├── victron_ble.h
│   ├── water/   
		├── water_level.cpp
        ├── water_level.h   
│   ├── weather/
		├── weather.cpp
        ├── weather.h   
bluetooth_config.h
config_base.h
```