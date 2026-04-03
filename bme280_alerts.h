#ifndef BME280_ALERTS_H
#define BME280_ALERTS_H

enum AlertType {
    ALERT_NONE = 0,
    ALERT_FREEZE,           // < 2°C
    ALERT_CONDENSATION,     // Point rosée proche
    ALERT_HIGH_HUMIDITY,    // > 70%
    ALERT_LOW_PRESSURE,     // < 980 hPa (tempête)
    ALERT_HIGH_TEMP         // > 35°C
};

struct Alert {
    AlertType type;
    const char* message;
    uint32_t triggeredAt;
    bool active;
    lv_color_t color;
};

class BME280Alerts {
public:
    void check(float temp, float humidity, float pressure, float dewPoint);
    Alert* getActiveAlerts(uint8_t& count);
    void acknowledge(AlertType type);
    
private:
    Alert alerts[5] = {
        {ALERT_FREEZE, "Risque de gel!", 0, false, lv_color_hex(0x3498db)},
        {ALERT_CONDENSATION, "Risque condensation", 0, false, lv_color_hex(0x9b59b6)},
        {ALERT_HIGH_HUMIDITY, "Humidité élevée", 0, false, lv_color_hex(0xe67e22)},
        {ALERT_LOW_PRESSURE, "Pression basse - Tempête?", 0, false, lv_color_hex(0xe74c3c)},
        {ALERT_HIGH_TEMP, "Température élevée", 0, false, lv_color_hex(0xc0392b)}
    };
    
    static constexpr float CONDENSATION_MARGIN = 3.0;  // °C
};

#endif