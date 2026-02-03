#pragma once

#include <stdint.h>

typedef struct {
    int x, y, w, h;

    char* buffer;
    int   max_len;
    int   len;

    int has_focus;

    int caret_pos;
    int caret_visible;
    uint32_t caret_last_toggle_ms;

    // Placeholder
    const char* placeholder;

    // Renkler
    uint32_t text_color;
    uint32_t placeholder_color;
    uint32_t bg_color;
    uint32_t border_focus;
    uint32_t border_nofocus;

    // PASSWORD MODU
    int  use_password_char;   // 0 = normal, 1 = password
    char password_char;       // ekranda gözükecek karakter (örn '*')

} ui_textbox_t;

void ui_textbox_init(ui_textbox_t* tb,
                     int x, int y, int w, int h,
                     char* buffer, int max_len);

void ui_textbox_set_placeholder(ui_textbox_t* tb, const char* text);

void ui_textbox_set_password_mode(ui_textbox_t* tb, int enabled);
void ui_textbox_set_password_char(ui_textbox_t* tb, char c);

void ui_textbox_set_colors(ui_textbox_t* tb,
                           uint32_t text_color,
                           uint32_t placeholder_color,
                           uint32_t bg_color,
                           uint32_t border_focus,
                           uint32_t border_nofocus);

void ui_textbox_update(ui_textbox_t* tb);
void ui_textbox_draw(const ui_textbox_t* tb);

// textbox.h sonuna ekle
void ui_textbox_on_key(ui_textbox_t* tb, uint16_t ev);
void ui_textbox_on_mouse(ui_textbox_t* tb, int mx, int my, uint8_t pressed, uint8_t buttons);
