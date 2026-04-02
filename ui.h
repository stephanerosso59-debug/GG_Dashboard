#pragma once
/*
 * ui.h - Gestionnaire LVGL global
 */
#include <Arduino.h>
#include <lvgl.h>

// Pages du dashboard
typedef enum {
    PAGE_HOME    = 0,
    PAGE_LIGHTS  = 1,
    PAGE_BATTERY = 2,
    PAGE_HEATING = 3,
    PAGE_SYSTEM  = 4,
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

    // Styles partages
    lv_style_t style_card;
    lv_style_t style_btn_nav;
    lv_style_t style_nav_btn;         // barre nav — inactif
    lv_style_t style_nav_btn_active;  // barre nav — page courante
    lv_style_t style_label_title;
    lv_style_t style_label_value;
    lv_style_t style_label_unit;

    // Labels de la barre de statut globale (overlay sur ecran)
    lv_obj_t* lbl_sb_bt    = nullptr;
    lv_obj_t* lbl_sb_time  = nullptr;
    lv_obj_t* lbl_sb_soc   = nullptr;
    lv_obj_t* lbl_sb_volt  = nullptr;

private:
    void _initStyles();
    void _buildAllPages();

    DashPage   _currentPage;
    lv_obj_t*  _pages[PAGE_COUNT];
};

extern UiManager ui;

// ---- Pages individuelles (forward declarations) ----
void ui_draw_bg(lv_obj_t* parent);  // Fond commun degrade sombre

void page_home_build(lv_obj_t* parent);
void page_home_update();
void page_status_update();  // mise a jour barre statut globale

void page_lights_build(lv_obj_t* parent);

void page_battery_build(lv_obj_t* parent);
void page_battery_update();

void page_heating_build(lv_obj_t* parent);
void page_heating_update();

void page_system_build(lv_obj_t* parent);
void page_system_update();
