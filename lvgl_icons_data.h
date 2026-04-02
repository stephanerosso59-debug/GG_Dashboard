// lvgl_icons_data.h  — STUBS de compilation
// Frames 1x1 px pour valider la compilation sans saturer la RAM.
// Remplacer par le vrai fichier genere par :
//   cd workspace_dwin/tools && python export_lvgl_icons.py
#pragma once
#include <stdint.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
  #include "lvgl.h"
#else
  #include <lvgl.h>
#endif

static const uint8_t _px_b[] = {0x1F,0x6F};
static const uint8_t _px_g[] = {0x2E,0xA0};
static const uint8_t _px_y[] = {0xF7,0xCC};
static const uint8_t _px_o[] = {0xE3,0x65};
static const uint8_t _px_r[] = {0x8B,0x94};

#define _F(n,px) static const lv_img_dsc_t n = { \
    .header={(lv_img_cf_t)LV_IMG_CF_TRUE_COLOR,0,0,1,1},.data_size=2,.data=px}

#define _ARR8(pfx,px) \
    _F(pfx##0,px);_F(pfx##1,px);_F(pfx##2,px);_F(pfx##3,px); \
    _F(pfx##4,px);_F(pfx##5,px);_F(pfx##6,px);_F(pfx##7,px); \
    static const lv_img_dsc_t* frames_##pfx[] = { \
        &pfx##0,&pfx##1,&pfx##2,&pfx##3, \
        &pfx##4,&pfx##5,&pfx##6,&pfx##7}; \
    static const uint8_t FRAMES_CNT_##pfx = 8

_ARR8(wifi_scan,   _px_b);
_ARR8(wifi_ok,     _px_g);
_ARR8(ble_scan,    _px_b);
_ARR8(ble_ok,      _px_g);
_ARR8(weather_sun, _px_y);
_ARR8(weather_rain,_px_b);
_ARR8(weather_snow,_px_r);
_ARR8(weather_storm,_px_y);
_ARR8(batt_charge, _px_g);
_ARR8(solar_flux,  _px_y);
_ARR8(heat_flame,  _px_o);

#define FRAMES_WIFI_SCAN_COUNT    8
#define FRAMES_WIFI_OK_COUNT      8
#define FRAMES_BLE_SCAN_COUNT     8
#define FRAMES_BLE_OK_COUNT       8
#define FRAMES_WEATHER_SUN_COUNT  8
#define FRAMES_WEATHER_RAIN_COUNT 8
#define FRAMES_WEATHER_SNOW_COUNT 8
#define FRAMES_WEATHER_STORM_COUNT 8
#define FRAMES_BATT_CHARGE_COUNT  8
#define FRAMES_SOLAR_FLUX_COUNT   8
#define FRAMES_HEAT_FLAME_COUNT   8
