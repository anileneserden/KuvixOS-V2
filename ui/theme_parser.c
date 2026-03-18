// src/themes/theme_parser.c
#include <ui/theme.h>
#include <stdint.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>

typedef enum {
    SEC_NONE,
    SEC_DESKTOP,
    SEC_WINDOW,
    SEC_WINDOW_BUTTONS,
    SEC_CURSOR,   // ileride istersen kullanırsın
    SEC_TEXTBOX,
    SEC_BUTTON,
    SEC_DOCK
} theme_section_t;

// Küçük yardımcı: hepsi boşluk/tab mı?
static int is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r');
}

// Hex karakter -> nibble (0..15), geçersizse -1
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// "#RRGGBB" veya "0xRRGGBB" veya "RRGGBB" -> 0x00RRGGBB (sende alpha yok)
static fb_color_t parse_color(const char* s, const char* end)
{
    while (s < end && is_space(*s)) s++;

    // optional prefixes
    if (s < end && *s == '#') {
        s++;
    } else if ((end - s) >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    if (end - s < 6) return fb_rgb(255,255,255);

    int r_hi = hex_val(s[0]), r_lo = hex_val(s[1]);
    int g_hi = hex_val(s[2]), g_lo = hex_val(s[3]);
    int b_hi = hex_val(s[4]), b_lo = hex_val(s[5]);
    if (r_hi < 0 || r_lo < 0 || g_hi < 0 || g_lo < 0 || b_hi < 0 || b_lo < 0)
        return fb_rgb(255,255,255);

    uint8_t r = (uint8_t)((r_hi << 4) | r_lo);
    uint8_t g = (uint8_t)((g_hi << 4) | g_lo);
    uint8_t b = (uint8_t)((b_hi << 4) | b_lo);
    return fb_rgb(r,g,b);
}

// Basit integer parse (pozitif, decimal)
static int parse_int(const char* s, const char* end)
{
    while (s < end && is_space(*s)) s++;
    int val = 0;
    while (s < end && *s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static const char* skip_ws(const char* s, const char* end){
    while (s < end && is_space(*s)) s++;
    return s;
}

static int streq_lit(const char* s, const char* end, const char* lit){
    s = skip_ws(s,end);
    int i=0;
    while (lit[i] && (s+i) < end) {
        if (s[i] != lit[i]) return 0;
        i++;
    }
    return lit[i] == 0; // literal bitti mi
}


// İki stringi sabit uzunlukla karşılaştır (küçük helper)
// line "bg = ..." gibi, key "bg", key_len = 2
static int line_starts_with(const char* line, const char* line_end,
                            const char* key, int key_len)
{
    if (line_end - line < key_len) return 0;
    for (int i = 0; i < key_len; i++) {
        if (line[i] != key[i]) return 0;
    }
    return 1;
}

void ui_theme_load_from_kth(const char* text, ui_theme_t* out)
{
    // Önce bazı default değerler verelim
    out->desktop_bg         = fb_rgb(0, 0, 0);
    out->window_bg          = fb_rgb(200, 200, 200);
    out->window_border      = fb_rgb(80, 80, 80);
    out->window_title_bg    = fb_rgb(40, 40, 90);
    out->window_title_text  = fb_rgb(255, 255, 255);
    out->window_corner_radius = 0;
    out->window_border_px = 2;
    out->window_title_h   = 24;

    out->dock_bg            = fb_rgba(32,32,32,200);
    out->dock_border        = fb_rgb(64,64,64);
    out->dock_icon_bg       = fb_rgb(48,48,48);
    out->dock_icon_hover_bg = fb_rgb(80,80,80);
    out->dock_height        = 56;
    out->dock_radius        = 28;
    out->dock_margin_bottom = 18;
    out->dock_padding_x     = 12;
    out->dock_gap           = 10;
    out->dock_icon_size     = 32;
    out->dock_spacing       = 10;
    out->dock_position      = 1;
    out->dock_auto_hide     = 0;

    out->window_title_align  = UI_ALIGN_LEFT;
    out->window_title_pad_l  = 10;
    out->window_title_pad_r  = 10;

    out->window_btn_style    = UI_BTN_STYLE_TRAFFIC;
    out->window_btn_layout   = UI_BTN_LAYOUT_RIGHT;

    out->window_btn_size     = 12;
    out->window_btn_gap      = 6;
    out->window_btn_margin   = 6;

    out->window_btn_pad_left  = 8;
    out->window_btn_pad_right = 8;

    // default order: close,max,min
    out->window_btn_order[0] = 0;
    out->window_btn_order[1] = 1;
    out->window_btn_order[2] = 2;

    out->window_btn_close = parse_color("#FF5F57", "#FF5F57"+7);
    out->window_btn_max   = parse_color("#28C840", "#28C840"+7);
    out->window_btn_min   = parse_color("#FEBC2E", "#FEBC2E"+7);

    out->window_btn_hover_dark = 15;
    out->window_btn_press_dark = 30;

    // Textbox default
    out->textbox_bg           = fb_rgb(239,239,239);
    out->textbox_border       = fb_rgb(170,170,170);
    out->textbox_focus_border = fb_rgb(80,128,255);
    out->textbox_text         = fb_rgb(0,0,0);
    out->textbox_placeholder  = fb_rgb(136,136,136);
    out->textbox_caret        = fb_rgb(0,0,0);

    // Button default
    out->button_bg            = fb_rgb(200,200,200);
    out->button_border        = fb_rgb(100,100,100);
    out->button_hover_bg      = fb_rgb(220,220,220);
    out->button_hover_border  = fb_rgb(80,80,80);
    out->button_pressed_bg    = fb_rgb(180,180,180);
    out->button_pressed_border= fb_rgb(60,60,60);
    out->button_text          = fb_rgb(0,0,0);


    theme_section_t sec = SEC_NONE;

    const char* p = text;
    while (*p) {
        const char* line = p;
        const char* nl   = line;

        // Satır sonunu bul
        while (*nl && *nl != '\n') nl++;

        // Satır [section] mi?
        if (line < nl && line[0] == '[') {
            // [desktop]
            if ((nl - line) >= 9 && line[1] == 'd' && line[2] == 'e' && line[3] == 's'
                && line[4] == 'k' && line[5] == 't' && line[6] == 'o'
                && line[7] == 'p' && line[8] == ']') {
                sec = SEC_DESKTOP;
            }
            // [window]
            else if ((nl - line) >= 8 && line[1] == 'w' && line[2] == 'i' && line[3] == 'n'
                     && line[4] == 'd' && line[5] == 'o' && line[6] == 'w'
                     && line[7] == ']') {
                sec = SEC_WINDOW;
            }
            // [cursor]
            else if ((nl - line) >= 8 && line[1] == 'c' && line[2] == 'u' && line[3] == 'r'
                     && line[4] == 's' && line[5] == 'o' && line[6] == 'r'
                     && line[7] == ']') {
                sec = SEC_CURSOR;
            }
            // [textbox]
            else if ((nl - line) >= 9 && line[1] == 't' && line[2] == 'e' && line[3] == 'x'
                     && line[4] == 't' && line[5] == 'b' && line[6] == 'o'
                     && line[7] == 'x' && line[8] == ']') {
                sec = SEC_TEXTBOX;
            }
            // [button]
            else if ((nl - line) >= 8 && line[1] == 'b' && line[2] == 'u' && line[3] == 't'
                     && line[4] == 't' && line[5] == 'o' && line[6] == 'n'
                     && line[7] == ']') {
                sec = SEC_BUTTON;
            }
            // [dock]
            else if ((nl - line) >= 6 && line[1]=='d' && line[2]=='o' && line[3]=='c' && line[4]=='k' && line[5]==']') {
                sec = SEC_DOCK;
            }
            else if ((nl - line) >= 16 && line[1]=='w' && line[2]=='i' && line[3]=='n' &&
                    line[4]=='d' && line[5]=='o' && line[6]=='w' && line[7]=='_' &&
                    line[8]=='b' && line[9]=='u' && line[10]=='t' && line[11]=='t' &&
                    line[12]=='o' && line[13]=='n' && line[14]=='s' && line[15]==']') {
                sec = SEC_WINDOW_BUTTONS;
            }
        }
        else {
            // key = value satırı
            // Eşittir'i bul
            const char* eq = line;
            while (eq < nl && *eq != '=') eq++;

            if (eq < nl) {
                // key: [line, eq)
                // value: [eq+1, nl)
                const char* key_start = line;
                const char* key_end   = eq - 1;

                // key sonundaki boşlukları at
                while (key_end >= key_start && is_space(*key_end)) key_end--;
                key_end++;

                const char* val_start = eq + 1;
                const char* val_end   = nl;

                // Bu satırın hangi anahtar olduğunu bul
                if (sec == SEC_DESKTOP) {
                    if (line_starts_with(key_start, key_end, "bg", 2)) {
                        out->desktop_bg = parse_color(val_start, val_end);
                    }
                }
                else if (sec == SEC_WINDOW) {

                    // renkler (hem bg hem window_bg kabul)
                    if (line_starts_with(key_start, key_end, "bg", 2) ||
                        line_starts_with(key_start, key_end, "window_bg", 9)) {
                        out->window_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "border", 6) ||
                            line_starts_with(key_start, key_end, "window_border", 12)) {
                        out->window_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_bg", 8) ||
                            line_starts_with(key_start, key_end, "window_title_bg", 15)) {
                        out->window_title_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_text", 10) ||
                            line_starts_with(key_start, key_end, "window_title_text", 17)) {
                        out->window_title_text = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "window_border_px", 16) ||
                            line_starts_with(key_start, key_end, "border_px", 9)) {
                        out->window_border_px = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "window_title_h", 14) ||
                            line_starts_with(key_start, key_end, "title_h", 7)) {
                        out->window_title_h = parse_int(val_start, val_end);
                    }
                    // int alanlar (hem title_h hem window_title_h kabul)
                    else if (line_starts_with(key_start, key_end, "border_px", 9) ||
                            line_starts_with(key_start, key_end, "window_border_px", 16)) {
                        out->window_border_px = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_h", 7) ||
                            line_starts_with(key_start, key_end, "window_title_h", 14)) {
                        out->window_title_h = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_pad_l", 11) ||
                            line_starts_with(key_start, key_end, "window_title_pad_l", 18)) {
                        out->window_title_pad_l = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_pad_r", 11) ||
                            line_starts_with(key_start, key_end, "window_title_pad_r", 18)) {
                        out->window_title_pad_r = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "corner_radius", 13) ||
                            line_starts_with(key_start, key_end, "window_corner_radius", 20)) {
                        out->window_corner_radius = parse_int(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "title_align", 11) ||
                            line_starts_with(key_start, key_end, "window_title_align", 18)) {
                        if (streq_lit(val_start, val_end, "left")) out->window_title_align = UI_ALIGN_LEFT;
                        else if (streq_lit(val_start, val_end, "center")) out->window_title_align = UI_ALIGN_CENTER;
                        else if (streq_lit(val_start, val_end, "right")) out->window_title_align = UI_ALIGN_RIGHT;
                    }
                }
                else if (sec == SEC_WINDOW_BUTTONS) {
                    if (line_starts_with(key_start, key_end, "layout", 6)) {
                        if (streq_lit(val_start,val_end,"left")) out->window_btn_layout = UI_BTN_LAYOUT_LEFT;
                        else if (streq_lit(val_start,val_end,"right")) out->window_btn_layout = UI_BTN_LAYOUT_RIGHT;
                    }
                    else if (line_starts_with(key_start, key_end, "style", 5)) {
                        if (streq_lit(val_start,val_end,"classic")) out->window_btn_style = UI_BTN_STYLE_CLASSIC;
                        else if (streq_lit(val_start,val_end,"traffic")) out->window_btn_style = UI_BTN_STYLE_TRAFFIC;
                    }
                    else if (line_starts_with(key_start, key_end, "size", 4)) {
                        out->window_btn_size = parse_int(val_start,val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "gap", 3)) {
                        out->window_btn_gap = parse_int(val_start,val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "margin", 6)) {
                        out->window_btn_margin = parse_int(val_start,val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "pad_left", 8)) {
                        out->window_btn_pad_left = parse_int(val_start,val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "pad_right", 9)) {
                        out->window_btn_pad_right = parse_int(val_start,val_end);
                    }
                    // order=close,max,min
                    else if (line_starts_with(key_start, key_end, "order", 5)) {
                        // basit parse: close/max/min sıralamasını 0/1/2 yap
                        // (istersen daha robust yaparız)
                        // default:
                        out->window_btn_order[0] = 0;
                        out->window_btn_order[1] = 1;
                        out->window_btn_order[2] = 2;

                        // çok basit: string içinde "min" nerede vs
                        // burada sen istersen sonra iyileştirirsin
                        if (strstr(val_start, "min") && strstr(val_start, "max") && strstr(val_start, "close")) {
                            // TODO: gerçek sıralama parse (şimdilik default kalsın)
                        }
                    }
                }
                else if (sec == SEC_TEXTBOX) {
                    if (line_starts_with(key_start, key_end, "bg", 2)) {
                        out->textbox_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "border", 6)) {
                        out->textbox_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "focus_border", 12)) {
                        out->textbox_focus_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "text", 4)) {
                        out->textbox_text = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "placeholder", 11)) {
                        out->textbox_placeholder = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "caret", 5)) {
                        out->textbox_caret = parse_color(val_start, val_end);
                    }
                }
                else if (sec == SEC_BUTTON) {
                    if (line_starts_with(key_start, key_end, "bg", 2)) {
                        out->button_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "border", 6)) {
                        out->button_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "hover_bg", 8)) {
                        out->button_hover_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "hover_border", 12)) {
                        out->button_hover_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "pressed_bg", 10)) {
                        out->button_pressed_bg = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "pressed_border", 14)) {
                        out->button_pressed_border = parse_color(val_start, val_end);
                    }
                    else if (line_starts_with(key_start, key_end, "text", 4)) {
                        out->button_text = parse_color(val_start, val_end);
                    }
                }
                else if (sec == SEC_DOCK) {
                    if (line_starts_with(key_start, key_end, "bg", 2)) {
                        out->dock_bg = parse_color(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "border", 6)) {
                        out->dock_border = parse_color(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "icon_bg", 7)) {
                        out->dock_icon_bg = parse_color(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "height", 6)) {
                        out->dock_height = parse_int(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "radius", 6)) {
                        out->dock_radius = parse_int(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "margin_bottom", 13)) {
                        out->dock_margin_bottom = parse_int(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "padding_x", 9)) {
                        out->dock_padding_x = parse_int(val_start, val_end);
                    } else if (line_starts_with(key_start, key_end, "gap", 3)) {
                        out->dock_gap = parse_int(val_start, val_end);
                    }
                }

            }
        }

        // Sıradaki satıra geç
        if (*nl == '\n') p = nl + 1;
        else            p = nl;
    }
}