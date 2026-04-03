#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

struct BME280Data {
    float temperature;    // °C
    float humidity;       // %
    float pressure;       // hPa
    float altitude;       // m (calculé)
    bool valid;
    uint32_t lastUpdate;
};

class BME280Sensor {
public:
    bool begin(uint8_t addr = 0x76, TwoWire *wire = &Wire);
    bool update();                    // Appeler dans loop()
    const BME280Data& getData() const { return data; }
    
    // Helpers
    float getTemperature() const { return data.temperature; }
    float getHumidity() const { return data.humidity; }
    float getPressure() const { return data.pressure; }
    bool isValid() const { return data.valid; }
    
    static constexpr uint32_t UPDATE_INTERVAL_MS = 2000; // 2s

private:
    Adafruit_BME280 bme;
    BME280Data data = {0};
    uint32_t lastReadTime = 0;
    bool initialized = false;
};

#endif