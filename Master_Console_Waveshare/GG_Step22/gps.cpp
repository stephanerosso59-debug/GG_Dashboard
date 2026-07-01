/*
 * gps.cpp — Parseur NMEA léger pour NEO-6M
 * 
 * Pas besoin de librairie externe TinyGPS++.
 * Implémentation minimale qui gère $GPRMC et $GPGGA.
 */
#include "gps.h"
#include <HardwareSerial.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

GpsData gpsData = {};

// Utilisation UART1 (HardwareSerial(1))
static HardwareSerial GpsUart(1);

// Buffer ligne NMEA
#define NMEA_MAX_LEN 128
static char nmea_buf[NMEA_MAX_LEN];
static size_t nmea_len = 0;

// ── Utils : convertir "ddmm.mmmm" NMEA → degrés décimaux ─
static float nmea_to_deg(const char* s, char dir) {
    if (!s || !*s) return 0;
    float raw = atof(s);
    int deg = (int)(raw / 100);
    float min = raw - deg * 100;
    float result = deg + min / 60.0f;
    if (dir == 'S' || dir == 'W') result = -result;
    return result;
}

// ── Split CSV dans buffer (modifie le buffer) ────────────
static int split_csv(char* line, char** fields, int max_fields) {
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while (*p && n < max_fields) {
        if (*p == ',') {
            *p = 0;
            fields[n++] = p + 1;
        }
        // Couper avant le checksum '*'
        if (*p == '*') {
            *p = 0;
            break;
        }
        p++;
    }
    return n;
}

// ── Parse $GPRMC (position + vitesse + date + heure) ─────
// Format: $GPRMC,hhmmss.ss,A,llll.llll,N,lllll.llll,E,speed,course,ddmmyy,...*hh
static void parse_GPRMC(char* line) {
    char* f[15];
    int n = split_csv(line, f, 15);
    if (n < 10) return;
    
    // f[1] = time HHMMSS.ss
    // f[2] = status A=valid V=void
    // f[3] = lat  f[4] = N/S
    // f[5] = lon  f[6] = E/W
    // f[7] = speed knots
    // f[8] = course deg
    // f[9] = date DDMMYY
    
    if (f[2][0] != 'A') {
        gpsData.has_fix = false;
        return;
    }
    gpsData.has_fix = true;
    
    // Heure
    if (strlen(f[1]) >= 6) {
        char hh[3] = {f[1][0], f[1][1], 0};
        char mm[3] = {f[1][2], f[1][3], 0};
        char ss[3] = {f[1][4], f[1][5], 0};
        gpsData.hour_utc   = atoi(hh);
        gpsData.minute_utc = atoi(mm);
        gpsData.second_utc = atoi(ss);
    }
    
    // Position
    gpsData.latitude  = nmea_to_deg(f[3], f[4][0]);
    gpsData.longitude = nmea_to_deg(f[5], f[6][0]);
    
    // Vitesse (knots → km/h)
    gpsData.speed_kmh = atof(f[7]) * 1.852f;
    gpsData.course_deg = atof(f[8]);
    
    // Date
    if (strlen(f[9]) >= 6) {
        char dd[3] = {f[9][0], f[9][1], 0};
        char mm[3] = {f[9][2], f[9][3], 0};
        char yy[3] = {f[9][4], f[9][5], 0};
        gpsData.day   = atoi(dd);
        gpsData.month = atoi(mm);
        gpsData.year  = 2000 + atoi(yy);
    }
    
    gpsData.valid = true;
    gpsData.last_update_ms = millis();
}

// ── Parse $GPGGA (altitude + nb satellites) ──────────────
// Format: $GPGGA,hhmmss.ss,lat,N,lon,E,fix,sats,hdop,alt,M,...*hh
static void parse_GPGGA(char* line) {
    char* f[16];
    int n = split_csv(line, f, 16);
    if (n < 10) return;
    
    // f[6] = fix quality (0=no fix)
    // f[7] = satellites
    // f[8] = HDOP
    // f[9] = altitude m
    
    gpsData.satellites = atoi(f[7]);
    gpsData.hdop       = atof(f[8]);
    gpsData.altitude_m = atof(f[9]);
}

// ── Process une ligne NMEA complète ──────────────────────
static void process_nmea(char* line) {
    if (line[0] != '$' || nmea_len < 10) return;
    
    if (strncmp(line + 3, "RMC", 3) == 0) {
        parse_GPRMC(line);
    } else if (strncmp(line + 3, "GGA", 3) == 0) {
        parse_GPGGA(line);
    }
}

// ============================================================
//  API
// ============================================================
void gps_init() {
    gpsData = {};
    
    Serial0.printf("[GPS] Init UART1 (RX=GPIO%d TX=GPIO%d @ %d bauds)...\n",
        GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    
    GpsUart.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    // Clear buffer
    while (GpsUart.available()) GpsUart.read();
    
    Serial0.println("[GPS] OK - en attente de fix...");
}

void gps_update() {
    // Lire tout ce qui est dispo (non-bloquant)
    while (GpsUart.available()) {
        char c = GpsUart.read();
        
        // Fin de ligne
        if (c == '\n' || c == '\r') {
            if (nmea_len > 5) {
                nmea_buf[nmea_len] = 0;
                process_nmea(nmea_buf);
            }
            nmea_len = 0;
        } else if (nmea_len < NMEA_MAX_LEN - 1) {
            nmea_buf[nmea_len++] = c;
        } else {
            // Overflow : reset
            nmea_len = 0;
        }
    }
    
    // Timeout : plus de fix si pas de donnees depuis 10s
    if (gpsData.valid && (millis() - gpsData.last_update_ms) > 10000) {
        gpsData.has_fix = false;
    }
}

bool gps_is_available() {
    return gpsData.valid && gpsData.has_fix
        && (millis() - gpsData.last_update_ms) < 10000;
}

// ── Distance entre 2 points (Haversine, km) ──────────────
float gps_distance_km(float lat1, float lon1, float lat2, float lon2) {
    float R = 6371.0f;
    float phi1 = lat1 * M_PI / 180.0f;
    float phi2 = lat2 * M_PI / 180.0f;
    float dphi = (lat2 - lat1) * M_PI / 180.0f;
    float dlmb = (lon2 - lon1) * M_PI / 180.0f;
    float a = sinf(dphi/2) * sinf(dphi/2)
            + cosf(phi1) * cosf(phi2) * sinf(dlmb/2) * sinf(dlmb/2);
    float c = 2 * atan2f(sqrtf(a), sqrtf(1-a));
    return R * c;
}

float gps_bearing_deg(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = lat1 * M_PI / 180.0f;
    float phi2 = lat2 * M_PI / 180.0f;
    float dlmb = (lon2 - lon1) * M_PI / 180.0f;
    float y = sinf(dlmb) * cosf(phi2);
    float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(dlmb);
    float brng = atan2f(y, x) * 180.0f / M_PI;
    return fmodf(brng + 360.0f, 360.0f);
}
