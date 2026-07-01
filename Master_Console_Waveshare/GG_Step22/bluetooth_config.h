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
#define JKBMS_MAC_ADDRESS       "XX:XX:XX:XX:XX:XX"

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
#define VICTRON_MPPT_MAC        "XX:XX:XX:XX:XX:XX"
#define VICTRON_MPPT_BINDKEY    "00000000000000000000000000000000"

// ============================================================
//  Victron SmartBMV 712 (Battery Monitor)
// ============================================================
#define VICTRON_BMV_ENABLED     true
#define VICTRON_BMV_MAC         "XX:XX:XX:XX:XX:XX"
#define VICTRON_BMV_BINDKEY     "00000000000000000000000000000000"

// Compatibilite legacy
#define VICTRON_SHUNT_ENABLED   VICTRON_BMV_ENABLED
#define VICTRON_SHUNT_MAC       VICTRON_BMV_MAC
#define VICTRON_SHUNT_BINDKEY   VICTRON_BMV_BINDKEY

// ============================================================
//  Victron BatteryProtect
// ============================================================
#define VICTRON_BP_ENABLED      true
#define VICTRON_BP_MAC          "XX:XX:XX:XX:XX:XX"
#define VICTRON_BP_BINDKEY      "00000000000000000000000000000000"

// ============================================================
//  Victron OrionSmart DC-DC
//
//  Note : partage la meme MAC que le SmartShunt (XX:XX:XX:XX:XX:XX).
//  C'est normal : Victron emet differents record_type (0x01 / 0x04)
//  depuis la meme adresse BLE. Le parsing differencie par readout_type.
// ============================================================
#define VICTRON_ORION_ENABLED   true
#define VICTRON_ORION_MAC       "XX:XX:XX:XX:XX:XX"
#define VICTRON_ORION_BINDKEY   "00000000000000000000000000000000"

// ============================================================
//  Victron SmartShunt (meme appareil physique que l'OrionSmart)
// ============================================================
#define VICTRON_SMARTSHUNT_ENABLED  true
#define VICTRON_SMARTSHUNT_MAC      "XX:XX:XX:XX:XX:XX"
#define VICTRON_SMARTSHUNT_BINDKEY  "00000000000000000000000000000000"

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
#define HEATING_BLE_MAC         "XX:XX:XX:XX:XX:XX"

// UUIDs a determiner avec nRF Connect une fois connecte :
//   VictronConnect -> nRF Connect -> Scanner -> cliquer l'appareil -> Services
// Valeurs provisoires (UUID generiques) :
#define HEATING_SERVICE_UUID    "0000fff0-0000-1000-8000-00805f9b34fb"
#define HEATING_CHAR_WRITE_UUID "0000fff1-0000-1000-8000-00805f9b34fb"
#define HEATING_CHAR_NOTIFY_UUID "0000fff2-0000-1000-8000-00805f9b34fb"

// Commandes (a ajuster apres identification du protocole)
#define HEATING_CMD_ON          0x01
#define HEATING_CMD_OFF         0x00
