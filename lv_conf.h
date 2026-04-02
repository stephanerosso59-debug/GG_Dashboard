/*
 * lv_conf.h - Configuration LVGL 8.x pour ESP32 Van Dashboard
 * Placer ce fichier dans le dossier lib/ ou src/
 */
#if 1  /* Active le fichier de config */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Couleur de l'ecran */
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   1   /* Pour TFT_eSPI */

/* Memoire */
#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (48U * 1024U)  /* 48 KB */
#define LV_MEM_ADR         0
#define LV_MEM_AUTO_DEFRAG 1

/* HAL */
#define LV_TICK_CUSTOM     1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Log */
#define LV_USE_LOG         0

/* Asserts */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_OBJ           0
#define LV_USE_ASSERT_STYLE         0

/* Resolution */
#define LV_HOR_RES_MAX              480
#define LV_VER_RES_MAX              320

/* Theme */
#define LV_USE_THEME_DEFAULT        1
#define LV_THEME_DEFAULT_DARK       1
#define LV_THEME_DEFAULT_GROW       1

/* Widgets */
#define LV_USE_ARC          1
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     1
#define LV_USE_CHART        1
#define LV_USE_COLORWHEEL   0
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1
#define LV_USE_IMGBTN       1
#define LV_USE_KEYBOARD     0
#define LV_USE_LABEL        1
#define LV_USE_LED          1
#define LV_USE_LINE         1
#define LV_USE_LIST         1
#define LV_USE_MENU         0
#define LV_USE_METER        1
#define LV_USE_MSGBOX       1
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       1
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      1
#define LV_USE_SWITCH       1
#define LV_USE_TABVIEW      1
#define LV_USE_TABLE        0
#define LV_USE_TEXTAREA     0
#define LV_USE_TILEVIEW     1
#define LV_USE_WIN          0

/* Fonts */
#define LV_FONT_MONTSERRAT_8      1
#define LV_FONT_MONTSERRAT_10     1
#define LV_FONT_MONTSERRAT_12     1
#define LV_FONT_MONTSERRAT_14     1
#define LV_FONT_MONTSERRAT_16     1
#define LV_FONT_MONTSERRAT_18     1
#define LV_FONT_MONTSERRAT_20     1
#define LV_FONT_MONTSERRAT_22     1
#define LV_FONT_MONTSERRAT_24     1
#define LV_FONT_MONTSERRAT_28     1
#define LV_FONT_MONTSERRAT_32     1
#define LV_FONT_MONTSERRAT_36     1
#define LV_FONT_MONTSERRAT_48     1
#define LV_FONT_DEFAULT           &lv_font_montserrat_14

/* Animations */
#define LV_USE_ANIMATION    1

/* Misc */
#define LV_DRAW_COMPLEX     1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

#define LV_USE_FS_STDIO     0
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0

#define LV_USE_PNG          0
#define LV_USE_BMP          0
#define LV_USE_SJPG         0
#define LV_USE_GIF          0

#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_GPU_SWAPPED_ENDIAN 0
#define LV_USE_MSG          0
#define LV_USE_IME_PINYIN   0

#endif /* LV_CONF_H */
#endif /* Fin de la condition Active */
