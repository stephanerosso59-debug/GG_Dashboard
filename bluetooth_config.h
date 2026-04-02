#pragma once
/*
 * bluetooth_config.h - Adresses MAC et cles BLE
 *
 * JKBMS :
 *   Adresse MAC visible dans l'app JK-BMS ou avec un scanner BLE
 *   (ex: nRF Connect, BLE Scanner)
 *
 * Victron :
 *   MAC + Bindkey (cle AES 32 caracteres hex)
 *   Bindkey dans VictronConnect > Appareil > Parametres > Info produit > Afficher
 *
 * Chauffage BLE :
 *   Appareil non-Victron (Webasto / Espar / thermostat)
 *   UUIDs GATT a identifier avec nRF Connect
 */

// ============================================================
//  JKBMS - BMS Lithium JK
// ============================================================
#define JKBMS_MAC_ADDRESS       "C8:47:80:01:CD:31"

// UUIDs services/caracteristiques JK-BMS
#define JKBMS_SERVICE_UUID      "0000ffe0-0000-1000-8000-00805f9b34fb"
#define JKBMS_CHAR_NOTIFY_UUID  "0000ffe1-0000-1000-8000-00805f9b34fb"
#define JKBMS_CHAR_WRITE_UUID   "0000ffe1-0000-1000-8000-00805f9b34fb"

// Trame de requete cellules (commande 0x96 = device info + cell data)
static const uint8_t JKBMS_CMD_CELL_DATA[] = {
    0xAA, 0x55, 0x90, 0xEB, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11
};

// Nombre max de cellules supportees
#define JKBMS_MAX_CELLS         24
// Intervalle de poll BLE JKBMS (ms)
#define JKBMS_POLL_INTERVAL_MS  5000

// ============================================================
//  Victron SmartSolar MPPT
// ============================================================
#define VICTRON_MPPT_ENABLED    true
#define VICTRON_MPPT_MAC        "CF:A0:F0:EF:C4:39"
#define VICTRON_MPPT_BINDKEY    "6372a8ef71c13912b58e28209d0c68f8"

// ============================================================
//  Victron SmartBMV 712 (Battery Monitor)
// ============================================================
#define VICTRON_BMV_ENABLED     true
#define VICTRON_BMV_MAC         "C5:9F:63:D2:E7:45"
#define VICTRON_BMV_BINDKEY     "133cd5da7fd7298d1e4e6a05a705b72b"

// Compatibilite legacy
#define VICTRON_SHUNT_ENABLED   VICTRON_BMV_ENABLED
#define VICTRON_SHUNT_MAC       VICTRON_BMV_MAC
#define VICTRON_SHUNT_BINDKEY   VICTRON_BMV_BINDKEY

// ============================================================
//  Victron BatteryProtect
// ============================================================
#define VICTRON_BP_ENABLED      true
#define VICTRON_BP_MAC          "E5:7A:2C:1C:D4:0C"
#define VICTRON_BP_BINDKEY      "607c044a97097b2f82e64438716cf310"

// ============================================================
//  Victron OrionSmart DC-DC
//
//  Note : partage la meme MAC que le SmartShunt (DE:5F:86:D7:15:E3).
//  C'est normal : Victron emet differents record_type (0x01 / 0x04)
//  depuis la meme adresse BLE. Le parsing differencie par readout_type.
// ============================================================
#define VICTRON_ORION_ENABLED   true
#define VICTRON_ORION_MAC       "DE:5F:86:D7:15:E3"
#define VICTRON_ORION_BINDKEY   "429abec6ad1a0fbe31305eeee50ebd20"

// ============================================================
//  Victron SmartShunt (meme appareil physique que l'OrionSmart)
// ============================================================
#define VICTRON_SMARTSHUNT_ENABLED  true
#define VICTRON_SMARTSHUNT_MAC      "DE:5F:86:D7:15:E3"
#define VICTRON_SMARTSHUNT_BINDKEY  "429abec6ad1a0fbe31305eeee50ebd20"

// ============================================================
//  Victron - Identifiant fabricant dans les paquets BLE
// ============================================================
#define VICTRON_MANUFACTURER_ID  0x02E1
// Intervalle de scan BLE Victron (ms)
#define VICTRON_SCAN_INTERVAL_MS 5000

// Types de records Victron (Instant Readout)
#define VICTRON_RECORD_SOLAR_CHARGER     0x01
#define VICTRON_RECORD_BATTERY_MONITOR   0x02
#define VICTRON_RECORD_INVERTER          0x03
#define VICTRON_RECORD_DCDC_CONVERTER    0x04
#define VICTRON_RECORD_SMART_LITHIUM     0x05
#define VICTRON_RECORD_BATTERY_PROTECT   0x06

// ============================================================
//  Chauffage BLE (Webasto / Espar / thermostat)
//  UUIDs GATT a identifier avec nRF Connect
// ============================================================
#define HEATING_BLE_MAC         "C1:02:29:4F:FE:50"

// UUIDs a determiner avec nRF Connect une fois connecte :
//   VictronConnect -> nRF Connect -> Scanner -> cliquer l'appareil -> Services
// Valeurs provisoires (UUID generiques) :
#define HEATING_SERVICE_UUID    "0000fff0-0000-1000-8000-00805f9b34fb"
#define HEATING_CHAR_WRITE_UUID "0000fff1-0000-1000-8000-00805f9b34fb"
#define HEATING_CHAR_NOTIFY_UUID "0000fff2-0000-1000-8000-00805f9b34fb"

// Commandes (a ajuster apres identification du protocole)
#define HEATING_CMD_ON          0x01
#define HEATING_CMD_OFF         0x00
