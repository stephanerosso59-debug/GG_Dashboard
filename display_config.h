#pragma once
/*
 * display_config.h - Configuration Multi-Écran GG Dashboard
 * 
 * Waveshare ESP32-S3-Touch-LCD-7B = 1024×600 (Type B, contrôleur ST7701)
 * ILI9341 = 480×320 (Type A, contrôleur ILI9341)
 */

/* ═══ SÉLECTION ÉCRAN (décommentez UN SEUL) ═══ */
#define DISPLAY_TFT_ILI9341      // ← ESP32 + ILI9341 480×320
// #define DISPLAY_WS7_1024x600   // ← ESP32-S3 + Waveshare 7B 1024×600 ✨


#if defined(DISPLAY_TFT_ILI9341)
    #define DISPLAY_NAME          "ILI9341 480x320"
    #define SCREEN_WIDTH           480
    #define SCREEN_HEIGHT          320
    #define SCREEN_ROTATION       1
    #define DISPLAY_PIXEL_COUNT    (480*320)   // 153,600
    #define USE_TFT_ESPI_DRIVER    1
    #define HAS_PSRAM               0
    #define HAS_CAPACITIVE_TOUCH  0
    #define STATUS_BAR_HEIGHT      22
    #define NAV_BAR_HEIGHT         50
    #define NEED_PAGE_NAVIGATION   1
    #define SHOW_DASHBOARD_VIEW    0
    #define FONT_SIZE_TITLE        20
    #define FONT_SIZE_VALUE        28
    #define FONT_SIZE_UNIT         14
    #define CARD_RADIUS            14
    #define GRID_COLS              2
    #define GRID_GAP                8
    #define LVGL_DRAW_BUF_LINES    40

#elif defined(DISPLAY_WS7_1024x600)
    /* ── WAVESHARE 7B TYPE B : 1024×600 (ST7701) ──────────────── */
    #define DISPLAY_NAME          "WS7B 1024x600"
    #define SCREEN_WIDTH           1024       ← CORRIGÉ : était 800
    #define SCREEN_HEIGHT          600       ← CORRIGÉ : était 480
    #define SCREEN_ROTATION       0         Landscape natif
    #define DISPLAY_PIXEL_COUNT    (1024*600) // 614,400 pixels !
    
    #define USE_WS7_ST7701_DRIVER   1       ← Contrôleur ST7701
    #define HAS_PSRAM               1
    #define PSRAM_SIZE_KB           8192     // 8 MB PSRAM
    #define HAS_CAPACITIVE_TOUCH  1        GT911 5 points I2C
    #define TOUCH_I2C_SDA          8
    #define TOUCH_I2C_SCL          9
    
    #define STATUS_BAR_HEIGHT      36        Plus grande barre status
    #define NAV_BAR_HEIGHT         0         Pas besoin nav bar
    #define NEED_PAGE_NAVIGATION   0
    #define SHOW_DASHBOARD_VIEW    1        Vue dashboard unique
    
    /* Fonts optimisés pour 1024×600 (plus grand = polices plus grandes) */
    #define FONT_SIZE_TITLE        32       ← Augmenté (était 28)
    #define FONT_SIZE_VALUE        48       ← Augmenté (était 42)
    #define FONT_SIZE_UNIT         22       ← Augmenté (était 20)
    #define FONT_SIZE_SMALL        16       ← Augmenté (était 14)
    #define FONT_SIZE_TINY         12       ← NOUVEAU
    
    #define CARD_RADIUS            20       ← Coins plus arrondis
    #define CARD_PADDING           18       ← Plus padding interne
    
    /* Grille 4 colonnes possible sur 1024px ! */
    #define GRID_COLS              4        ← 4 colonnes (était 3)
    #define GRID_GAP                16       ← Plus d'espacement
    
    #define LVGL_DRAW_BUF_LINES    80       ← Plus de lignes buffer
    #define LVGL_BUF_SIZE          (1024*80*2) // ~160KB → utiliser PSRAM

#else
    #error "Define DISPLAY_TFT_ILI9341 or DISPLAY_WS7_1024x600"
#endif

/* ═══ CONSTANTES CALCULÉES AUTOMATIQUEMENT ═══ */
#define CONTENT_WIDTH  SCREEN_WIDTH
#define CONTENT_HEIGHT (SCREEN_HEIGHT - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT)
#define SCALE_X (((float)SCREEN_WIDTH)/480.0f)
#define SCALE_Y (((float)SCREEN_HEIGHT)/320.0f)
#define SX(val) ((lv_coord_t)((val)*SCALE_X))  // 1024/480 = 2.133x
#define SY(val) ((lv_coord_t)((val)*SCALE_Y))  // 600/320 = 1.875x
#define COL_WIDTH ((CONTENT_WIDTH-(GRID_COLS-1)*GRID_GAP)/GRID_COLS)
#define COL(idx) ((idx)*(COL_WIDTH+GRID_GAP))

/* BME280 I2C (commun aux deux écrans) */
#define BME280_SDA_PIN 21
#define BME280_SCL_PIN 22
#define BME280_I2C_ADDR 0x76
#define ALERT_FREEZE_TEMP 2.0f
#define ALERT_HIGH_TEMP 35.0f
#define ALERT_HIGH_HUMIDITY 70.0f
#define ALERT_LOW_PRESSURE 980.0f
#define ALERT_CONDENSATION_MARGIN 3.0f

#include "../../shared/config_base.h"