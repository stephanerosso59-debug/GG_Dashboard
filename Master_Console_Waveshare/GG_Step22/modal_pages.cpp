/*
 * modal_pages.cpp - Pages detail en mode "modal" (creees a la demande, detruites au retour)
 * Evite l'OOM en n'allouant la RAM que quand on en a besoin.
 */
#include "ui.h"
#include "lv_fonts_fr.h"
#include "icons_van.h"
#include "display_config.h"
#include "bme_wire1.h"
#include "scd41.h"
#include "weather.h"
#include "weather_icons.h"
#include "icons_sd.h"
#include "gps.h"
#include "modal_pages.h"
#include <math.h>

// La page modale courante (nullptr si aucune)
static lv_obj_t* current_modal = nullptr;

// Type de modale (pour les updates)
enum ModalType { MODAL_NONE, MODAL_GPS, MODAL_INTERIOR, MODAL_WEATHER, MODAL_WEATHER7 };
static ModalType current_type = MODAL_NONE;
static int weather_day = 0;  // 0=auj, 1=dem, 2=apr

// Widgets dynamiques (pointeurs sur les labels a updater)
static lv_obj_t* m_lbl1 = nullptr;
static lv_obj_t* m_lbl2 = nullptr;
static lv_obj_t* m_lbl3 = nullptr;
static lv_obj_t* m_lbl4 = nullptr;
static lv_obj_t* m_lbl5 = nullptr;
static lv_obj_t* m_lbl6 = nullptr;
static lv_obj_t* m_status = nullptr;

// ──────────────────────────────────────────────────────
// Fermeture (label retour)
// ──────────────────────────────────────────────────────
static void modal_close_cb(lv_event_t* e) {
    if (current_modal) {
        lv_obj_del(current_modal);
        current_modal = nullptr;
        current_type = MODAL_NONE;
        m_lbl1 = m_lbl2 = m_lbl3 = m_lbl4 = m_lbl5 = m_lbl6 = m_status = nullptr;
    }
}

// ──────────────────────────────────────────────────────
// Helper : créer une étiquette simple à une position
// ──────────────────────────────────────────────────────
static lv_obj_t* make_lbl(lv_obj_t* parent, int x, int y, const char* txt,
                          const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

// ──────────────────────────────────────────────────────
// Construction du fond modal commun
// ──────────────────────────────────────────────────────
static lv_obj_t* build_modal_base(const char* titre) {
    lv_obj_t* screen = lv_scr_act();
    
    // Fond plein écran (par-dessus tout)
    lv_obj_t* modal = lv_obj_create(screen);
    lv_obj_set_size(modal, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(modal, 0, 0);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_set_style_radius(modal, 0, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    
    // Bouton Retour (label simple cliquable)
    lv_obj_t* lbl_back = lv_label_create(modal);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT "  Retour");
    lv_obj_set_pos(lbl_back, 16, 16);
    lv_obj_set_style_text_font(lbl_back, &FONT_22, 0);
    lv_obj_set_style_text_color(lbl_back, COLOR_BLUE, 0);
    lv_obj_add_flag(lbl_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_back, modal_close_cb, LV_EVENT_CLICKED, nullptr);
    
    // Titre centré
    lv_obj_t* title_lbl = lv_label_create(modal);
    lv_label_set_text(title_lbl, titre);
    lv_obj_set_style_text_font(title_lbl, &FONT_36, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_BLUE, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 14);
    
    return modal;
}

// ──────────────────────────────────────────────────────
// MODAL GPS
// ──────────────────────────────────────────────────────
void modal_gps_open() {
    if (current_modal) modal_close_cb(nullptr);
    
    current_modal = build_modal_base("Position GPS");
    current_type = MODAL_GPS;
    
    m_status = make_lbl(current_modal, 100, 100, LV_SYMBOL_GPS "  Recherche signal...",
                       &FONT_28, COLOR_YELLOW);
    
    make_lbl(current_modal, 100, 180, "Latitude", &FONT_18, COLOR_GRAY);
    m_lbl1 = make_lbl(current_modal, 100, 210, "--.----- deg", &FONT_28, COLOR_WHITE);
    
    make_lbl(current_modal, 500, 180, "Longitude", &FONT_18, COLOR_GRAY);
    m_lbl2 = make_lbl(current_modal, 500, 210, "--.----- deg", &FONT_28, COLOR_WHITE);
    
    make_lbl(current_modal, 100, 290, "Altitude", &FONT_18, COLOR_GRAY);
    m_lbl3 = make_lbl(current_modal, 100, 320, "-- m", &FONT_28, COLOR_GREEN);
    
    make_lbl(current_modal, 350, 290, "Vitesse", &FONT_18, COLOR_GRAY);
    m_lbl4 = make_lbl(current_modal, 350, 320, "-- km/h", &FONT_28, COLOR_BLUE);
    
    make_lbl(current_modal, 600, 290, "Satellites", &FONT_18, COLOR_GRAY);
    m_lbl5 = make_lbl(current_modal, 600, 320, "--", &FONT_28, COLOR_ORANGE);
    
    Serial0.println("[Modal] GPS detail ouvert");
}

// ──────────────────────────────────────────────────────
// MODAL INTERIEUR
// ──────────────────────────────────────────────────────
void modal_interior_open() {
    if (current_modal) modal_close_cb(nullptr);
    
    current_modal = build_modal_base("Conditions interieures");
    current_type = MODAL_INTERIOR;
    
    int y = 130;
    
    make_lbl(current_modal, 80, y, "Temperature", &FONT_18, COLOR_GRAY);
    m_lbl1 = make_lbl(current_modal, 80, y+30, "-- C", &FONT_48, COLOR_ORANGE);
    
    make_lbl(current_modal, 360, y, "Humidite", &FONT_18, COLOR_GRAY);
    m_lbl2 = make_lbl(current_modal, 360, y+30, "-- %", &FONT_48, COLOR_BLUE);
    
    make_lbl(current_modal, 640, y, "Pression", &FONT_18, COLOR_GRAY);
    m_lbl3 = make_lbl(current_modal, 640, y+30, "-- hPa", &FONT_36, COLOR_GRAY);
    
    int y2 = y + 200;
    make_lbl(current_modal, 80, y2, "CO2", &FONT_18, COLOR_GRAY);
    m_lbl4 = make_lbl(current_modal, 80, y2+30, "-- ppm", &FONT_48, COLOR_GREEN);
    m_lbl5 = make_lbl(current_modal, 80, y2+90, "Inactif", &FONT_22, COLOR_GRAY);
    
    Serial0.println("[Modal] Interieur ouvert");
}

// ──────────────────────────────────────────────────────
// MODAL METEO
// ──────────────────────────────────────────────────────
void modal_weather_open(int day) {
    if (current_modal) modal_close_cb(nullptr);
    
    weather_day = day;
    const char* titre = (day == 0) ? "Aujourd'hui" :
                       (day == 1) ? "Demain" : "Apres-demain";
    
    current_modal = build_modal_base(titre);
    current_type = MODAL_WEATHER;
    
    m_lbl1 = make_lbl(current_modal, 100, 130, "-- C", &FONT_48, COLOR_ORANGE);
    m_lbl2 = make_lbl(current_modal, 100, 230, "Inconnu", &FONT_28, COLOR_WHITE);
    
    make_lbl(current_modal, 100, 320, "Humidite", &FONT_18, COLOR_GRAY);
    m_lbl3 = make_lbl(current_modal, 100, 350, "-- %", &FONT_36, COLOR_BLUE);
    
    make_lbl(current_modal, 500, 320, "Vent", &FONT_18, COLOR_GRAY);
    m_lbl4 = make_lbl(current_modal, 500, 350, "-- km/h", &FONT_36, COLOR_GREEN);
    
    // Grosse icone meteo a droite (init avec icone par defaut pour eviter plantage)
    m_lbl5 = lv_img_create(current_modal);
    lv_img_set_zoom(m_lbl5, 768);  // 64*768/256 = 192px
    lv_obj_set_pos(m_lbl5, 720, 130);
    lv_obj_clear_flag(m_lbl5, LV_OBJ_FLAG_CLICKABLE);
    
    // Charger une icone par defaut tout de suite (sinon widget vide => crash)
    const lv_img_dsc_t* default_ic = icons_sd_get("weather_sunny");
    if (default_ic) lv_img_set_src(m_lbl5, default_ic);
    
    // Si on a deja les donnees meteo, mettre la bonne icone
    if (weatherData.valid) {
        WeatherDay* d = (day == 0) ? &weatherData.today :
                       (day == 1) ? &weatherData.tomorrow :
                                    &weatherData.after;
        const lv_img_dsc_t* ic = icons_sd_get(owm_to_icon(d->icon));
        if (ic) lv_img_set_src(m_lbl5, ic);
    }
    
    // ── Bouton "Voir 7 jours" en bas a droite ──
    lv_obj_t* btn7 = lv_label_create(current_modal);
    lv_label_set_text(btn7, ">> Voir 7 jours >>");
    lv_obj_set_style_text_font(btn7, &FONT_22, 0);
    lv_obj_set_style_text_color(btn7, lv_color_hex(0x55BBFF), 0);
    lv_obj_set_pos(btn7, 700, 510);
    lv_obj_add_flag(btn7, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn7, [](lv_event_t* e) {
        modal_weather7_open();  // Ouvre la modal 7 jours
    }, LV_EVENT_CLICKED, nullptr);
    
    Serial0.println("[Modal] Meteo ouvert");
}

// ────────────────────────────────────────────────────────
// Modal Météo 7 jours (Open-Meteo)
// ────────────────────────────────────────────────────────
void modal_weather7_open() {
    if (current_modal) modal_close_cb(nullptr);
    
    current_modal = build_modal_base("Meteo 7 jours");
    current_type = MODAL_WEATHER7;
    
    extern WeatherData weatherData;
    
    if (!weatherData.valid7days) {
        make_lbl(current_modal, 380, 280, "Donnees pas encore chargees...", &FONT_22, COLOR_GRAY);
        make_lbl(current_modal, 380, 320, "(connexion WiFi necessaire)", &FONT_18, COLOR_GRAY);
        Serial0.println("[Modal] Meteo 7j ouvert (donnees absentes)");
        return;
    }
    
    // Layout : 7 colonnes pour les 7 jours
    // Largeur ecran 1024, on garde 50px de chaque cote = 924 utiles
    // 7 colonnes * 130px = 910
    int col_w = 130;
    int x_start = 60;
    int y_top = 130;
    
    static const char* DAY_FRENCH[] = {
        "Aujour.", "Demain", "+2 jours", "+3 jours", "+4 jours", "+5 jours", "+6 jours"
    };
    
    for (int i = 0; i < 7; i++) {
        int x = x_start + i * col_w;
        WeatherDay* d = &weatherData.days7[i];
        
        // Label jour (en haut)
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%s", DAY_FRENCH[i]);
        make_lbl(current_modal, x, y_top, lbl, &FONT_18, COLOR_GRAY);
        
        // Icone meteo (centre)
        lv_obj_t* img = lv_img_create(current_modal);
        const lv_img_dsc_t* ic = icons_sd_get(owm_to_icon(d->icon));
        if (ic) lv_img_set_src(img, ic);
        lv_img_set_zoom(img, 240);  // 64*240/256 = 60px
        lv_obj_set_pos(img, x, y_top + 30);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
        
        // Temperatures min/max (en dessous icone)
        char tmax[16], tmin[16];
        snprintf(tmax, sizeof(tmax), "%.0f°", d->temp_max);
        snprintf(tmin, sizeof(tmin), "%.0f°", d->temp_min);
        
        make_lbl(current_modal, x, y_top + 110, tmax, &FONT_28, COLOR_ORANGE);
        make_lbl(current_modal, x + 50, y_top + 120, tmin, &FONT_22, lv_color_hex(0x88AAFF));
        
        // Condition (texte court)
        make_lbl(current_modal, x, y_top + 160, d->condition, &FONT_14, COLOR_WHITE);
        
        // Pluie (mm) si > 0
        if (d->rain_mm > 0.1f) {
            char rain[16];
            snprintf(rain, sizeof(rain), "%.1f mm", d->rain_mm);
            make_lbl(current_modal, x, y_top + 190, rain, &FONT_14, lv_color_hex(0x55AAFF));
        }
        
        // Vent (km/h)
        char wind[20];
        snprintf(wind, sizeof(wind), "%.0f km/h", d->wind_kmh);
        make_lbl(current_modal, x, y_top + 220, wind, &FONT_14, COLOR_GRAY);
    }
    
    // Source en bas
    make_lbl(current_modal, 380, 510, "Source: Open-Meteo.com (gratuit)", &FONT_14, COLOR_GRAY);
    
    Serial0.println("[Modal] Meteo 7j ouvert");
}

// ──────────────────────────────────────────────────────
// Update (appelée régulièrement par le loop)
// ──────────────────────────────────────────────────────
void modal_update() {
    if (!current_modal) return;
    char buf[64];
    
    if (current_type == MODAL_GPS) {
        if (gpsData.has_fix) {
            if (m_status) {
                lv_label_set_text(m_status, LV_SYMBOL_OK "  Fix GPS valide");
                lv_obj_set_style_text_color(m_status, COLOR_GREEN, 0);
            }
            if (m_lbl1) {
                snprintf(buf, sizeof(buf), "%.6f %s", fabs(gpsData.latitude),
                        gpsData.latitude >= 0 ? "N" : "S");
                lv_label_set_text(m_lbl1, buf);
            }
            if (m_lbl2) {
                snprintf(buf, sizeof(buf), "%.6f %s", fabs(gpsData.longitude),
                        gpsData.longitude >= 0 ? "E" : "O");
                lv_label_set_text(m_lbl2, buf);
            }
            if (m_lbl3) {
                snprintf(buf, sizeof(buf), "%d m", (int)gpsData.altitude_m);
                lv_label_set_text(m_lbl3, buf);
            }
            if (m_lbl4) {
                snprintf(buf, sizeof(buf), "%.1f km/h", gpsData.speed_kmh);
                lv_label_set_text(m_lbl4, buf);
            }
        }
        if (m_lbl5) {
            snprintf(buf, sizeof(buf), "%d", gpsData.satellites);
            lv_label_set_text(m_lbl5, buf);
        }
    }
    else if (current_type == MODAL_INTERIOR) {
        if (bmeData.valid) {
            if (m_lbl1) { snprintf(buf, sizeof(buf), "%.1f C", bmeData.temperature); lv_label_set_text(m_lbl1, buf); }
            if (m_lbl2) { snprintf(buf, sizeof(buf), "%.0f %%", bmeData.humidity); lv_label_set_text(m_lbl2, buf); }
            if (m_lbl3) { snprintf(buf, sizeof(buf), "%.0f hPa", bmeData.pressure); lv_label_set_text(m_lbl3, buf); }
        }
        if (scd41Data.valid && m_lbl4 && m_lbl5) {
            snprintf(buf, sizeof(buf), "%d ppm", scd41Data.co2_ppm);
            lv_label_set_text(m_lbl4, buf);
            if (scd41Data.co2_ppm < 800) {
                lv_obj_set_style_text_color(m_lbl4, COLOR_GREEN, 0);
                lv_label_set_text(m_lbl5, "Air pur");
            } else if (scd41Data.co2_ppm < 1200) {
                lv_obj_set_style_text_color(m_lbl4, COLOR_YELLOW, 0);
                lv_label_set_text(m_lbl5, "Acceptable");
            } else if (scd41Data.co2_ppm < 2000) {
                lv_obj_set_style_text_color(m_lbl4, COLOR_ORANGE, 0);
                lv_label_set_text(m_lbl5, "Aerez");
            } else {
                lv_obj_set_style_text_color(m_lbl4, COLOR_RED, 0);
                lv_label_set_text(m_lbl5, "Critique");
            }
        }
    }
    else if (current_type == MODAL_WEATHER) {
        WeatherDay* day = (weather_day == 0) ? &weatherData.today :
                         (weather_day == 1) ? &weatherData.tomorrow :
                                              &weatherData.after;
        if (weatherData.valid) {
            if (m_lbl1) { snprintf(buf, sizeof(buf), "%.1f C", day->temp_c); lv_label_set_text(m_lbl1, buf); }
            if (m_lbl2) lv_label_set_text(m_lbl2, day->condition);
            if (m_lbl3) { snprintf(buf, sizeof(buf), "%d %%", day->humidity_pct); lv_label_set_text(m_lbl3, buf); }
            if (m_lbl4) { snprintf(buf, sizeof(buf), "%.0f km/h", day->wind_kmh); lv_label_set_text(m_lbl4, buf); }
            if (m_lbl5) {
                const lv_img_dsc_t* ic = icons_sd_get(owm_to_icon(day->icon));
                if (ic) lv_img_set_src(m_lbl5, ic);
            }
        }
    }
}

bool modal_is_open() {
    return current_modal != nullptr;
}
