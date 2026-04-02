#pragma once
// gps.h — Module GPS NEO-6M via UART2 (TinyGPS++)
// Fournit position, vitesse, cap et altitude pour l'affichage page accueil.

#include <Arduino.h>

// ─── Données GPS publiques ────────────────────────────────────────────────────
struct GpsData {
    bool    valid;          // Fix GPS valide
    double  lat;            // Latitude  (degrés décimaux)
    double  lon;            // Longitude (degrés décimaux)
    float   speed_kmh;      // Vitesse (km/h)
    float   course_deg;     // Cap (0-360°)
    float   altitude_m;     // Altitude (mètres)
    uint8_t satellites;     // Nombre de satellites
    uint8_t hdop;           // Précision horizontale (×10, 10 = 1.0)
    char    lat_str[12];    // Ex: "48.8566°N"
    char    lon_str[12];    // Ex: "2.3522°E"
    char    speed_str[8];   // Ex: "72 km/h"
    char    course_str[4];  // Ex: "NE" / "S" / "OSO"
};

extern GpsData gpsData;

// ─── API publique ─────────────────────────────────────────────────────────────
void gps_init();            // À appeler dans setup()
void gps_update();          // À appeler toutes les secondes dans loop()
const char* gps_cardinal(float deg);  // Retourne "N","NE","E"...
