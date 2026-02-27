#ifndef UI_COMBOBOX2_H
#define UI_COMBOBOX2_H

#include <stdint.h>
#include <stdbool.h>
#include <ui/controls/control.h>

typedef void (*ui_combobox2_on_change_t)(void* user, int index, const char* text);

typedef struct {
    // geometry (client-space)
    int x, y, w, h;

    // data
    const char* items[64];
    int item_count;
    int selected;          // -1 none

    // state
    bool open;
    int hover_index;       // dropdown hover (optional)

    // styling
    uint32_t bg;
    uint32_t border;
    uint32_t text_col;
    uint32_t sel_bg;
    uint32_t sel_text;

    // callbacks
    ui_combobox2_on_change_t on_change;
    void* on_change_user;

    // layout
    int item_h;            // dropdown item height (default 22)
    int max_visible;       // default 8 (scroll yoksa)
    ui_control_t base;
} ui_combobox2_t;

void ui_combobox2_init(ui_combobox2_t* cb, int x, int y, int w, int h);
void ui_combobox2_add_item(ui_combobox2_t* cb, const char* text);
void ui_combobox2_set_selected(ui_combobox2_t* cb, int index);
int  ui_combobox2_get_selected(const ui_combobox2_t* cb);
const char* ui_combobox2_get_selected_text(const ui_combobox2_t* cb);

void ui_combobox2_set_on_change(ui_combobox2_t* cb, ui_combobox2_on_change_t fn, void* user);

void ui_combobox2_draw(ui_combobox2_t* cb);
bool ui_combobox2_handle_mouse(ui_combobox2_t* cb, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons);

#endif