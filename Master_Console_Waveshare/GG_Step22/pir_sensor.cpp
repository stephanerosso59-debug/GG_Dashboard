/*
 * pir_sensor.cpp — PIR motion detection (delegue ON/OFF a screen_sleep.cpp)
 */
#include "pir_sensor.h"
#include "screen_sleep.h"
#include <Arduino.h>

bool pirLastState  = false;
bool displayOn     = true;
uint32_t lastMotionMs = 0;

void pir_init() {
    pinMode(PIR_GPIO, INPUT_PULLDOWN);
    lastMotionMs = millis();
    displayOn = true;
    Serial0.printf("[PIR] Init GPIO%d - delegue a screen_sleep\n", PIR_GPIO);
}

void display_force_on() {
    screen_sleep_wake();   // wake commun
    lastMotionMs = millis();
}

void pir_update() {
    bool motion = (digitalRead(PIR_GPIO) == HIGH);
    
    if (motion) {
        if (!pirLastState) {
            Serial0.println("[PIR] Mouvement detecte");
        }
        lastMotionMs = millis();
        pirLastState = true;
        
        // Reveiller l'ecran via screen_sleep
        screen_sleep_wake();
    } else {
        pirLastState = false;
    }
    
    // Note : la gestion timeout/dimming est faite par screen_sleep_update()
    displayOn = screen_is_awake();
}
