# Van Dashboard - Projet multi-workspace

Projet dashboard pour van amenage ESP32.
3 implementations independantes selon l'ecran disponible.

## Architecture

```
gg dashboard/
├── shared/              Code BLE + meteo commun (source unique)
│   ├── ble/             JKBMS + Victron NimBLE
│   ├── weather/         OpenWeatherMap API
│   ├── config_base.h    Constantes communes (EDITER ICI)
│   └── bluetooth_config.h
│
├── workspace_lvgl/      ESP32 + TFT ILI9341 + LVGL animations
├── workspace_dwin/      ESP32 + Ecran DWIN DGUS2 UART
├── workspace_esphome/   ESP32 + ESPHome + Home Assistant
│
└── mockup/
    └── mockup.html      Preview HTML des 5 pages avec animations
```

## Choisir votre workspace

| Workspace          | Ecran             | Avantages                      |
|--------------------|-------------------|--------------------------------|
| `workspace_lvgl`   | TFT ILI9341 SPI   | Animations riches, LVGL natif  |
| `workspace_dwin`   | DWIN DGUS2 UART   | Simple, robuste, plug-and-play |
| `workspace_esphome`| TFT ou DWIN       | Integration Home Assistant     |

## Premiere utilisation

1. **Editer** `shared/config_base.h` avec WiFi, MACs BLE, cles Victron
2. **Choisir** un workspace et suivre son README.md
3. **Flasher** l'ESP32

## Prerequis materiels

- ESP32 WROOM-32 ou WROVER
- JKBMS avec BLE (serie JK-BD*)
- Victron Smart MPPT + SmartShunt (BLE)
- Relais 8 canaux (lumieres + TV + chauffage)
- Ecran selon workspace (TFT ou DWIN)

## .gitignore recommande

```gitignore
workspace_esphome/secrets.yaml
workspace_esphome/icons/
*.bin
*.o
.pio/
__pycache__/
```
