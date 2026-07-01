/*
 * page_lights.cpp — Contrôle des relais avec icônes dédiées
 */
#include "ui.h"
#include "lv_fonts_fr.h"
#include "icons_van.h"
#include "icons_sd.h"
#include "display_config.h"
#include "water_state.h"
#include "espnow_master.h"

struct LightInfo {
    const char* name;
    const char* icon;       // UTF-8 FA (fallback)
    const char* sd_icon;    // Nom icone BMP sur SD
    int         relay_id;
    bool        state;
};

static LightInfo lights[] = {
    { "Salon",        LV_SYMBOL_HOME,    "light_salon",    1, false },
    { "Cuisine",      LV_SYMBOL_LIST,    "light_kitchen",  2, false },
    { "Chambre",      LV_SYMBOL_EYE_CLOSE, "light_bedroom",  3, false },
    { "Douche",       LV_SYMBOL_TINT,    "light_shower",   4, false },
    { "Ext. Avant",   LV_SYMBOL_GPS,     "light_ext",      5, false },
    { "TV",           LV_SYMBOL_VIDEO,   "light_tv",       6, false },
    { "Pompe eau",    LV_SYMBOL_DOWNLOAD,"light_pump",     7, false },
    { "Chauffe-eau",  LV_SYMBOL_POWER,   "light_hotwater", 8, false },
};
static const int N_LIGHTS = sizeof(lights) / sizeof(lights[0]);

static lv_obj_t* btn_refs[8] = {nullptr};
static lv_obj_t* icon_refs[8] = {nullptr};
static lv_obj_t* lbl_refs[8] = {nullptr};

static void _update_btn_visual(int idx) {
    if (!btn_refs[idx]) return;
    if (lights[idx].state) {
        lv_obj_set_style_bg_color(btn_refs[idx], COLOR_YELLOW, 0);
        lv_obj_set_style_border_color(btn_refs[idx], COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(btn_refs[idx], 3, 0);
        if (icon_refs[idx]) lv_obj_set_style_text_color(icon_refs[idx], lv_color_black(), 0);
        if (lbl_refs[idx])  lv_obj_set_style_text_color(lbl_refs[idx],  lv_color_black(), 0);
    } else {
        lv_obj_set_style_bg_color(btn_refs[idx], lv_color_hex(0x21262D), 0);
        lv_obj_set_style_border_color(btn_refs[idx], lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(btn_refs[idx], 1, 0);
        if (icon_refs[idx]) lv_obj_set_style_text_color(icon_refs[idx], COLOR_ACCENT, 0);
        if (lbl_refs[idx])  lv_obj_set_style_text_color(lbl_refs[idx],  COLOR_WHITE, 0);
    }
}

static void _light_btn_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= N_LIGHTS) return;
    lights[idx].state = !lights[idx].state;
    _update_btn_visual(idx);
    Serial0.printf("[Lights] %s → %s (relay %d)\n",
        lights[idx].name, lights[idx].state ? "ON" : "OFF", lights[idx].relay_id);
    
    // Construire le bitmask des 8 relais (relay_id 1..8 = bit 0..7)
    // relay_id 8 = Chauffe-eau → géré séparément via champ dédié
    uint8_t bitmask     = 0;
    uint8_t chauffe_eau = 0;
    for (int i = 0; i < N_LIGHTS; i++) {
        if (!lights[i].state) continue;
        if (lights[i].relay_id >= 1 && lights[i].relay_id <= 7) {
            bitmask |= (1 << (lights[i].relay_id - 1));
        } else if (lights[i].relay_id == 8) {
            chauffe_eau = 1;   // MMBT2222A K1 GPIO25 slave
        }
    }

    // Envoyer via ESP-NOW au PCB slave
    espnow_send_relays(bitmask, chauffe_eau);
    
    // Mise a jour optimiste de waterState
    if (lights[idx].relay_id == 7) {       // Pompe eau (relais 7)
        water_set_pump(lights[idx].state);
    } else if (lights[idx].relay_id == 8) { // Chauffe-eau (relais 8)
        water_set_heater(lights[idx].state);
    }
}

void page_lights_build(lv_obj_t* parent) {
    ui_draw_bg(parent);

    // Titre
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Lumières & Relais");
    lv_obj_set_style_text_font(title, &FONT_36, 0);
    lv_obj_set_style_text_color(title, COLOR_YELLOW, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Sous-titre
    lv_obj_t* sub = lv_label_create(parent);
    lv_label_set_text(sub, "Cliquez pour activer / désactiver");
    lv_obj_set_style_text_font(sub, &FONT_16, 0);
    lv_obj_set_style_text_color(sub, COLOR_GRAY, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 58);

    // Grille 4x2
    int grid_x0 = 15;
    int grid_y0 = 95;
    int btn_w   = 240;
    int btn_h   = 170;
    int gap_x   = 6;
    int gap_y   = 10;

    for (int i = 0; i < N_LIGHTS; i++) {
        int col = i % 4;
        int row = i / 4;
        int x = grid_x0 + col * (btn_w + gap_x);
        int y = grid_y0 + row * (btn_h + gap_y);

        // Bouton conteneur
        lv_obj_t* btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x21262D), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, _light_btn_cb, LV_EVENT_CLICKED, nullptr);
        btn_refs[i] = btn;

        // Icône : BMP depuis SD si dispo, sinon police
        const lv_img_dsc_t* sd_icon = icons_sd_get(lights[i].sd_icon);
        if (sd_icon) {
            lv_obj_t* img = lv_img_create(btn);
            lv_img_set_src(img, sd_icon);
            // 64x64 -> ~80px (zoom 320 = 125%)
            lv_img_set_zoom(img, 320);
            lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 16);
            lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
            icon_refs[i] = img;
        } else {
            lv_obj_t* lbl_ic = lv_label_create(btn);
            lv_label_set_text(lbl_ic, lights[i].icon);
            lv_obj_set_style_text_font(lbl_ic, &FONT_36, 0);
            lv_obj_set_style_text_color(lbl_ic, COLOR_ACCENT, 0);
            lv_obj_align(lbl_ic, LV_ALIGN_TOP_MID, 0, 16);
            icon_refs[i] = lbl_ic;
        }

        // Label texte
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, lights[i].name);
        lv_obj_set_style_text_font(lbl, &FONT_22, 0);
        lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -12);
        lbl_refs[i] = lbl;
    }
}
