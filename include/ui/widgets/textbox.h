#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x, y, w, h;

    char*  buf;
    int    cap;     // buffer capacity
    int    len;     // current length
    int    caret;   // caret index

    bool   focused;
    bool   hovered;

    // simple blink
    uint32_t last_blink_ms;
    bool     caret_on;

    // style
    uint32_t bg;
    uint32_t fg;
    uint32_t border;
    uint32_t border_focus;
} ui_textbox_t;

void ui_textbox_init(ui_textbox_t* tb, int x, int y, int w, int h, char* buffer, int capacity);

// Call each frame
void ui_textbox_draw(ui_textbox_t* tb);

// Mouse: mx,my are client-relative coordinates
void ui_textbox_mouse(ui_textbox_t* tb, int mx, int my, uint8_t pressed, uint8_t released);

// Key: scancode + ascii (you already have ascii via kbd_scancode_to_ascii)
void ui_textbox_key(ui_textbox_t* tb, uint16_t scancode, char ascii);

// helpers
bool ui_textbox_hit(ui_textbox_t* tb, int mx, int my);