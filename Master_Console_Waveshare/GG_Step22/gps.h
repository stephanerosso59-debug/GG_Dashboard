#pragma once
/*
 * gps.h — Lecture GPS NEO-6M via UART
 * 
 * Module NEO-6M (GY-GPS6MV2) :
 *   VCC → 3.3V (ou 5V)
 *   GND → GND
 *   TX  → ESP32 RX (GPS_RX_PIN)
 *   RX  → ESP32 TX (GPS_TX_PIN)  [optionnel, pas utilise]
 * 
 * Protocole NMEA 0183 à 9600 bauds
 * Trames utilisées : $GPRMC (position + vitesse + heure), $GPGGA (position + altitude + satellites)
 */
#include <Arduino.h>
#include <stdint.h>

// ── GPIO configurables (à adapter selon ton câblage) ─────
// Pour Waveshare ESP32-S3 7B :
//   - Connecteur "UART" (TX0/RX0) est déjà utilisé pour le debug USB
//   - On utilise UART1 ou UART2 sur des pins libres
//   - GPIO 17/18 : libres (si non utilisés par autre chose)
#define GPS_RX_PIN  17    // RX ESP32 (connecter au TX du GPS)
#define GPS_TX_PIN  18    // TX ESP32 (connecter au RX du GPS - optionnel)
#define GPS_BAUD    9600

struct GpsData {
    bool     valid;                 // position 2D/3D valide
    bool     has_fix;
    uint32_t last_update_ms;
    
    float    latitude;              // degrés décimaux (positif = Nord)
    float    longitude;             // degrés décimaux (positif = Est)
    float    altitude_m;            // mètres au-dessus du niveau de la mer
    float    speed_kmh;             // vitesse au sol
    float    course_deg;            // cap (0 = nord, 90 = est)
    
    int      satellites;            // nb satellites utilisés
    float    hdop;                  // precision horizontale
    
    // Heure GPS
    int      hour_utc;
    int      minute_utc;
    int      second_utc;
    int      day;
    int      month;
    int      year;
};

extern GpsData gpsData;

void gps_init();
void gps_update();                    // à appeler dans la loop

// Utilitaires
bool gps_is_available();              // true si fix récent (<10s)
float gps_distance_km(float lat1, float lon1, float lat2, float lon2);
float gps_bearing_deg(float lat1, float lon1, float lat2, float lon2);
