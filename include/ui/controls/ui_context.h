#pragma once
#include <ui/controls/control.h>
#include <ui/theme/theme.h>
#include <stdbool.h>

#define UI_MAX_ROOTS 256

typedef struct {
    ui_control_t* roots[UI_MAX_ROOTS];
    int root_count;

    int mouse_x, mouse_y;
    bool mouse_down;

    ui_control_t* hot;     // hovered control
    ui_control_t* active;  // pressed control

    bool has_dirty;
    int dirty;             // ✅ en azından derlensin

    const ui_theme_t* theme;
} ui_context_t;

void ui_ctx_init(ui_context_t* ui);
bool ui_ctx_add_root(ui_context_t* ui, ui_control_t* c);
void ui_ctx_draw(ui_context_t* ui);
void ui_ctx_mouse(ui_context_t* ui, int mx, int my, bool left_down);