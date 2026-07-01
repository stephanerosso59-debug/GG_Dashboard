#pragma once
/*
 * ui.h - Gestionnaire LVGL global avec navigation 5 pages
 */
#include <Arduino.h>
#include "lv_fonts_fr.h"
#include "icons_van.h"
#include <lvgl.h>
#include <stdint.h>

// Pages du dashboard
typedef enum {
    PAGE_HOME     = 0,
    PAGE_LIGHTS   = 1,
    PAGE_BATTERY  = 2,
    PAGE_HEATING  = 3,
    PAGE_HISTORY  = 4,
    PAGE_SYSTEM   = 5,
    PAGE_COUNT
} DashPage;

// Couleurs du theme van
#define COLOR_BG_DARK    lv_color_hex(0x0D1117)
#define COLOR_BG_CARD    lv_color_hex(0x161B22)
#define COLOR_ACCENT     lv_color_hex(0xF0A500)
#define COLOR_GREEN      lv_color_hex(0x2EA043)
#define COLOR_RED        lv_color_hex(0xDA3633)
#define COLOR_BLUE       lv_color_hex(0x1F6FEB)
#define COLOR_GRAY       lv_color_hex(0x8B949E)
#define COLOR_WHITE      lv_color_hex(0xE6EDF3)
#define COLOR_ORANGE     lv_color_hex(0xE3652A)
#define COLOR_YELLOW     lv_color_hex(0xF7CC00)

class UiManager {
public:
    void begin();
    void update();

    void showPage(DashPage page);
    DashPage currentPage() const { return _currentPage; }

    lv_style_t style_card;
    lv_style_t style_nav_btn;
    lv_style_t style_nav_btn_active;

    lv_obj_t* lbl_sb_bt    = nullptr;
    lv_obj_t* lbl_sb_time  = nullptr;
    lv_obj_t* lbl_sb_soc   = nullptr;
    lv_obj_t* lbl_sb_volt  = nullptr;

private:
    void _initStyles();
    void _buildAllPages();
    void _buildNavBar();

    DashPage   _currentPage;
    lv_obj_t*  _pages[PAGE_COUNT];
    lv_obj_t*  _nav_btns[PAGE_COUNT];
};

extern UiManager ui;

void ui_draw_bg(lv_obj_t* parent);
void ui_start_pulse(lv_obj_t* obj);
void ui_stop_pulse(lv_obj_t* obj);
void page_home_build(lv_obj_t* parent);
void page_home_update();
void page_lights_build(lv_obj_t* parent);
void page_battery_build(lv_obj_t* parent);
void page_battery_update();
void page_heating_build(lv_obj_t* parent);
void page_heating_update();
void page_history_build(lv_obj_t* parent);
void page_history_update();
void page_system_build(lv_obj_t* parent);
void page_system_update();
