// gps.cpp — Module GPS NEO-6M via UART2 (TinyGPS++)
#include "gps.h"
#include "../config.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

static TinyGPSPlus  _gps;
static HardwareSerial _gpsSerial(2);  // UART2

GpsData gpsData = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
const char* gps_cardinal(float deg) {
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSO","SO","OSO","O","ONO","NO","NNO","N"
    };
    int idx = (int)((deg + 11.25f) / 22.5f) % 16;
    return dirs[idx];
}

static void _format_coord(double val, bool is_lat, char* out, int len) {
    char hem = is_lat ? (val >= 0 ? 'N' : 'S') : (val >= 0 ? 'E' : 'O');
    snprintf(out, len, "%.4f°%c", fabs(val), hem);
}

// ─── API ─────────────────────────────────────────────────────────────────────
void gps_init() {
    _gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    memset(&gpsData, 0, sizeof(gpsData));
    snprintf(gpsData.lat_str,   sizeof(gpsData.lat_str),   "--°N");
    snprintf(gpsData.lon_str,   sizeof(gpsData.lon_str),   "--°E");
    snprintf(gpsData.speed_str, sizeof(gpsData.speed_str), "-- km/h");
    snprintf(gpsData.course_str,sizeof(gpsData.course_str),"--");
}

void gps_update() {
    // Consommer toutes les trames disponibles
    while (_gpsSerial.available()) {
        _gps.encode(_gpsSerial.read());
    }

    gpsData.valid      = _gps.location.isValid() && _gps.location.age() < 3000;
    gpsData.satellites = (uint8_t)_gps.satellites.value();
    gpsData.hdop       = (uint8_t)min((int)(_gps.hdop.value() / 10), 99);

    if (gpsData.valid) {
        gpsData.lat       = _gps.location.lat();
        gpsData.lon       = _gps.location.lng();
        gpsData.speed_kmh = (float)_gps.speed.kmph();
        gpsData.course_deg= (float)_gps.course.deg();
        gpsData.altitude_m= (float)_gps.altitude.meters();

        _format_coord(gpsData.lat, true,  gpsData.lat_str,    sizeof(gpsData.lat_str));
        _format_coord(gpsData.lon, false, gpsData.lon_str,    sizeof(gpsData.lon_str));
        snprintf(gpsData.speed_str,  sizeof(gpsData.speed_str),  "%d km/h", (int)gpsData.speed_kmh);
        snprintf(gpsData.course_str, sizeof(gpsData.course_str), "%s", gps_cardinal(gpsData.course_deg));
    } else {
        snprintf(gpsData.lat_str,   sizeof(gpsData.lat_str),   "Pas de fix");
        snprintf(gpsData.lon_str,   sizeof(gpsData.lon_str),   "");
        snprintf(gpsData.speed_str, sizeof(gpsData.speed_str), "-- km/h");
        snprintf(gpsData.course_str,sizeof(gpsData.course_str),"--");
    }
}
