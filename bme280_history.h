#ifndef BME280_HISTORY_H
#define BME280_HISTORY_H

#include <circular_buffer.h>

struct HistoryPoint {
    uint32_t timestamp;
    float temperature;
    float humidity;
    float pressure;
};

class BME280History {
public:
    static constexpr uint8_t SAMPLES_PER_HOUR = 2;  // Toutes les 30min
    static constexpr uint8_t HOURS_HISTORY = 24;
    static constexpr uint8_t MAX_SAMPLES = 48;
    
    void addSample(float temp, float hum, float press);
    const HistoryPoint* getData() const { return buffer; }
    uint8_t getCount() const { return count; }
    
    float getMinTemp() const;
    float getMaxTemp() const;
    float getTempTrend() const;  // + montante, - descendante
    
private:
    HistoryPoint buffer[MAX_SAMPLES];
    uint8_t index = 0;
    uint8_t count = 0;
};

// Point de rosée
inline float calculateDewPoint(float temp, float humidity) {
    float a = 17.271;
    float b = 237.7;
    float alpha = ((a * temp) / (b + temp)) + log(humidity / 100.0);
    return (b * alpha) / (a - alpha);
}

#endif