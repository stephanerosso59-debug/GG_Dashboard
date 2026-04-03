#pragma once
/*
 * dwin_vp_map.h - Carte des adresses Variable Pointer (VP) DWIN DGUS2
 *
 * Les ecrans DWIN communiquent via UART avec un protocole proprietaire.
 * Chaque donnee affichee ou lue est identifiee par une adresse VP (16 bits).
 *
 * PROTOCOL TRAME :
 *   Ecriture VP : 5A A5 [len] 82 [VP_H] [VP_L] [data...] (1 mot = 2 octets)
 *   Lecture VP  : 5A A5 04   83 [VP_H] [VP_L] [nb_mots]
 *   Chgt de page: 5A A5 07   82 00 84  5A 01 [page_id] 00
 *   Retour DWIN : 5A A5 [len] 83 [VP_H] [VP_L] [nb_mots] [data...]
 *
 * CONVENTION :
 *   - Les valeurs entieres sont stockees en big-endian sur 2 octets (1 mot)
 *   - Les virgules fixes utilisent un multiplicateur (ex: tension x100 = cV)
 *   - Les chaines de caracteres (ICO/TXT) occupent plusieurs mots consecutifs
 *
 * Pages DWIN :
 *   Page 0 : Boot / Splash
 *   Page 1 : Accueil (heure, meteo, connectivite, navigation)
 *   Page 2 : Eclairages & TV
 *   Page 3 : Energie (JKBMS + Victron)
 *   Page 4 : Chauffage
 *   Page 5 : Systeme ESP32
 */

// ============================================================
//  ADRESSES SYSTEME DWIN
// ============================================================
#define DWIN_VP_PAGE_CURRENT   0x0014   // Page active (registre systeme)
#define DWIN_VP_BACKLIGHT      0x0082   // Retroeclairage (0-100)
#define DWIN_VP_BUZZER         0x00A0   // Buzzer (1=bip court)

// ============================================================
//  PAGE 1 - ACCUEIL
// ============================================================

// Horloge
#define DWIN_VP_CLOCK_HOUR     0x1000   // Heure  (uint16)
#define DWIN_VP_CLOCK_MIN      0x1001   // Minute (uint16)
#define DWIN_VP_CLOCK_SEC      0x1002   // Seconde (uint16)
#define DWIN_VP_DATE_TEXT      0x1010   // Date en texte (8 mots = 16 chars ASCII)

// Connectivite - icones d'etat (0=off, 1=ok, 2=erreur)
#define DWIN_VP_WIFI_STATUS    0x1020   // 0=off 1=ok 2=faible
#define DWIN_VP_WIFI_RSSI      0x1021   // RSSI en dBm (int16)
#define DWIN_VP_BLE_STATUS     0x1022   // 0=off 1=JKBMS 2=JKBMS+Vic 3=tous
#define DWIN_VP_IP_TEXT        0x1030   // Adresse IP texte (8 mots)

// Meteo - Jour 0 (Aujourd'hui)
#define DWIN_VP_WTH0_ICON      0x1040   // ID icone meteo (0-9)
#define DWIN_VP_WTH0_TMAX      0x1041   // Temperature max x10 (ex: 185 = 18.5°C)
#define DWIN_VP_WTH0_TMIN      0x1042   // Temperature min x10
#define DWIN_VP_WTH0_HUMID     0x1043   // Humidite %

// Meteo - Jour 1 (Demain)
#define DWIN_VP_WTH1_ICON      0x1044
#define DWIN_VP_WTH1_TMAX      0x1045
#define DWIN_VP_WTH1_TMIN      0x1046
#define DWIN_VP_WTH1_HUMID     0x1047

// Meteo - Jour 2 (J+2)
#define DWIN_VP_WTH2_ICON      0x1048
#define DWIN_VP_WTH2_TMAX      0x1049
#define DWIN_VP_WTH2_TMIN      0x104A
#define DWIN_VP_WTH2_HUMID     0x104B

// Boutons de navigation (touches tactiles -> envoyees par le DWIN)
#define DWIN_VP_BTN_NAV        0x1050   // 1=Lumieres 2=Energie 3=Chauffage 4=Systeme

// ============================================================
//  PAGE 2 - ECLAIRAGES
// ============================================================

// Etat relais (bitmask uint16 : bit0=L1 bit1=L2...bit5=L6 bit6=TV)
#define DWIN_VP_RELAY_STATE    0x2000

// Touches tactiles ON/OFF par relais (retour DWIN)
#define DWIN_VP_BTN_LIGHT1     0x2010   // 0=off 1=on
#define DWIN_VP_BTN_LIGHT2     0x2011
#define DWIN_VP_BTN_LIGHT3     0x2012
#define DWIN_VP_BTN_LIGHT4     0x2013
#define DWIN_VP_BTN_LIGHT5     0x2014
#define DWIN_VP_BTN_LIGHT6     0x2015
#define DWIN_VP_BTN_TV         0x2016
#define DWIN_VP_BTN_ALL_OFF    0x2020   // Bouton "Tout eteindre"

// ============================================================
//  PAGE 3 - ENERGIE
// ============================================================

// --- JKBMS ---
#define DWIN_VP_BAT_SOC        0x3000   // SOC % (0-100)
#define DWIN_VP_BAT_VOLTAGE    0x3001   // Tension x100 cV (ex: 5216 = 52.16V)
#define DWIN_VP_BAT_CURRENT    0x3002   // Courant x10 dA (signe int16, ex: -150 = -15.0A)
#define DWIN_VP_BAT_POWER      0x3003   // Puissance W (int16)
#define DWIN_VP_BAT_REMAIN_AH  0x3004   // Capacite restante x10 (ex: 980 = 98.0Ah)
#define DWIN_VP_BAT_TEMP_MOS   0x3005   // Temperature MOS x10
#define DWIN_VP_BAT_TEMP1      0x3006   // Temperature sonde 1 x10
#define DWIN_VP_BAT_CYCLES     0x3007   // Cycles (uint16)
#define DWIN_VP_BAT_BALANCE    0x3008   // 0=off 1=actif

// Cellules
#define DWIN_VP_CELL_MIN_V     0x3010   // Tension min cellule x1000 mV (ex: 3450 = 3.450V)
#define DWIN_VP_CELL_MAX_V     0x3011   // Tension max cellule x1000 mV
#define DWIN_VP_CELL_AVG_V     0x3012   // Tension moy cellule x1000 mV
#define DWIN_VP_CELL_DELTA_MV  0x3013   // Ecart max-min en mV
#define DWIN_VP_CELL_COUNT     0x3014   // Nombre de cellules

// Alarmes JKBMS (bitmask)
// bit0=surtension bit1=sous-tension bit2=surcourant bit3=surchauffe bit4=court-circuit
#define DWIN_VP_BAT_ALARMS     0x3020

// --- Victron MPPT ---
#define DWIN_VP_MPPT_STATE     0x3030   // 0=Off 3=Bulk 4=Absorpt. 5=Float 7=Egalisation
#define DWIN_VP_MPPT_PV_VOLT   0x3031   // Tension PV x100 cV
#define DWIN_VP_MPPT_PV_POWER  0x3032   // Puissance PV W
#define DWIN_VP_MPPT_BAT_VOLT  0x3033   // Tension bat via MPPT x100 cV
#define DWIN_VP_MPPT_CHG_CURR  0x3034   // Courant charge x10 dA
#define DWIN_VP_MPPT_CHG_PWR   0x3035   // Puissance charge W
#define DWIN_VP_MPPT_YIELD_DAY 0x3036   // Production du jour Wh
#define DWIN_VP_MPPT_ERROR     0x3037   // Code erreur (0=OK)

// --- SmartShunt ---
#define DWIN_VP_SHUNT_SOC      0x3040   // SOC % x10 (ex: 956 = 95.6%)
#define DWIN_VP_SHUNT_VOLTAGE  0x3041   // Tension x100 cV
#define DWIN_VP_SHUNT_CURRENT  0x3042   // Courant x100 cA (int16)
#define DWIN_VP_SHUNT_POWER    0x3043   // Puissance W (int16)
#define DWIN_VP_SHUNT_TTG      0x3044   // Autonomie en minutes (0xFFFF = infini)
#define DWIN_VP_SHUNT_AH_USED  0x3045   // Ah consommes x10
#define DWIN_VP_SHUNT_ALARM    0x3046   // 0=OK 1=Alarme

// ============================================================
//  PAGE 4 - CHAUFFAGE
// ============================================================

#define DWIN_VP_HEAT_STATE     0x4000   // 0=Off 1=On
#define DWIN_VP_HEAT_TARGET    0x4001   // Consigne x10 (ex: 190 = 19.0°C)
#define DWIN_VP_HEAT_BTN_ON    0x4010   // Bouton ON (retour DWIN)
#define DWIN_VP_HEAT_BTN_OFF   0x4011   // Bouton OFF
#define DWIN_VP_HEAT_BTN_UP    0x4012   // Bouton + consigne
#define DWIN_VP_HEAT_BTN_DOWN  0x4013   // Bouton - consigne

// ============================================================
//  PAGE 5 - SYSTEME ESP32
// ============================================================

#define DWIN_VP_SYS_WIFI_SSID  0x5000   // SSID (16 chars = 8 mots)
#define DWIN_VP_SYS_WIFI_IP    0x5008   // IP (16 chars)
#define DWIN_VP_SYS_WIFI_MAC   0x5010   // MAC (12 chars)
#define DWIN_VP_SYS_RSSI       0x5018   // RSSI dBm (int16)
#define DWIN_VP_SYS_UPTIME_H   0x5020   // Uptime heures
#define DWIN_VP_SYS_UPTIME_M   0x5021   // Uptime minutes
#define DWIN_VP_SYS_FREE_HEAP  0x5022   // RAM libre KB
#define DWIN_VP_SYS_CPU_TEMP   0x5023   // Temp CPU x10
#define DWIN_VP_SYS_VERSION    0x5030   // Version ESPHome (16 chars)
#define DWIN_VP_SYS_CHIP_INFO  0x5038   // Info chip (16 chars)
#define DWIN_VP_SYS_BLE_JKBMS 0x5040   // 0=deconnecte 1=connecte
#define DWIN_VP_SYS_BLE_MPPT  0x5041   // 0=pas de donnees 1=ok
#define DWIN_VP_SYS_BLE_SHUNT 0x5042   // 0=pas de donnees 1=ok
#define DWIN_VP_BTN_REBOOT     0x5050   // Bouton redemarrage

// ============================================================
//  CONSTANTES PROTOCOLE DWIN
// ============================================================
#define DWIN_HEAD_H   0x5A
#define DWIN_HEAD_L   0xA5
#define DWIN_CMD_WRITE_VP  0x82
#define DWIN_CMD_READ_VP   0x83
#define DWIN_CMD_WRITE_REG 0x80
#define DWIN_CMD_READ_REG  0x81

// IDs icones meteo (correspondant aux images stockees sur le DWIN)
#define DWIN_WEATHER_SUNNY      0
#define DWIN_WEATHER_PARTLY     1
#define DWIN_WEATHER_CLOUDY     2
#define DWIN_WEATHER_OVERCAST   3
#define DWIN_WEATHER_DRIZZLE    4
#define DWIN_WEATHER_RAIN       5
#define DWIN_WEATHER_STORM      6
#define DWIN_WEATHER_SNOW       7
#define DWIN_WEATHER_FOG        8
#define DWIN_WEATHER_UNKNOWN    9
