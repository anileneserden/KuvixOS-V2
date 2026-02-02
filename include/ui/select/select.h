#pragma once
#include <stdint.h>
#include <kernel/drivers/video/fb.h>   // fb_color_t

typedef struct {
    int x, y, w, h;

    const char** items;
    int count;

    int selected;     // seçili index
    int open;         // 0 kapalı, 1 açık
    int hover;        // açıkken hover index, yoksa -1
    int has_focus;

    int item_h;       // dropdown satır yüksekliği (px)
} ui_select_t;

void ui_select_init(ui_select_t* s, int x, int y, int w, int h,
                    const char** items, int count, int initial_index);

void ui_select_set_items(ui_select_t* s, const char** items, int count, int initial_index);

void ui_select_update(ui_select_t* s, int mx, int my);

// mouse: pressed bitleri sende nasılsa ona göre (sen 0x01 sol click kullanıyorsun)
int  ui_select_on_mouse(ui_select_t* s, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons);

// key: senin uint16 event formatına göre (ascii low byte varsayımıyla)
int  ui_select_on_key(ui_select_t* s, uint16_t ev);

void ui_select_draw(const ui_select_t* s);

int         ui_select_get_selected(const ui_select_t* s);
const char* ui_select_get_selected_text(const ui_select_t* s);
