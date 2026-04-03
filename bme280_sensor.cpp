#include "bme280_sensor.h"

bool BME280Sensor::begin(uint8_t addr, TwoWire *wire) {
    if (!bme.begin(addr, wire)) {
        // Essayer adresse alternative 0x77
        if (!bme.begin(0x77, wire)) {
            Serial.println("[BME280] Capteur non trouvé!");
            data.valid = false;
            return false;
        }
    }
    
    // Configuration optimale van/camping-car
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X2,  // Temp
                    Adafruit_BME280::SAMPLING_X16, // Pression (important pour altitude)
                    Adafruit_BME280::SAMPLING_X1,  // Humidité
                    Adafruit_BME280::FILTER_X16,
                    Adafruit_BME280::STANDBY_MS_0_5);
    
    initialized = true;
    data.valid = true;
    Serial.println("[BME280] Initialisé");
    return true;
}

bool BME280Sensor::update() {
    if (!initialized) return false;
    
    uint32_t now = millis();
    if (now - lastReadTime < UPDATE_INTERVAL_MS) return data.valid;
    
    lastReadTime = now;
    
    // Mode forced = lecture sur demande (économie énergie)
    bme.takeForcedMeasurement();
    delay(10); // Attente conversion
    
    data.temperature = bme.readTemperature();
    data.humidity = bme.readHumidity();
    data.pressure = bme.readPressure() / 100.0F; // Pa → hPa
    data.altitude = bme.readAltitude(1013.25);   // Pression mer niveau
    data.lastUpdate = now;
    data.valid = (data.temperature != 0 || data.humidity != 0);
    
    return data.valid;
}