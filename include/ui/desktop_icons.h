#ifndef DESKTOP_ICONS_H
#define DESKTOP_ICONS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x, y;
    char label[32];
    char vfs_name[64];
    bool is_selected;
    bool is_dir;
    bool dragging;

    bool is_editing;
    char edit_buffer[32];
} desktop_icon_t;

typedef struct {
    int card_width;
    int card_height;
    int corner_radius;

    int grid_cell;
    int offset_x;
    int offset_y;

    int icon_base;
    int icon_scale;
    int icon_pad_top;

    int label_pad_bottom;
    int label_max_chars;

    uint32_t shadow_color;
    uint32_t normal_color;
    uint32_t hover_color;
    uint32_t selected_color;
    uint32_t text_color;
} desktop_icons_style_t;

// Temel Yönetim
void desktop_icons_init(void);
void desktop_icons_draw_all(void);
int  desktop_icons_get_hit(int mx, int my);
void desktop_icons_process_click(int index);
void desktop_icons_snap_all(void);
int desktop_icons_get_count(void);
const char* desktop_icons_get_name(int index);

// Style
void desktop_icons_set_style(const desktop_icons_style_t* style);
const desktop_icons_style_t* desktop_icons_get_style(void);

// Sürükleme
void desktop_icons_set_dragging(int index, bool state, int mx, int my);
void desktop_icons_move_dragging(int mx, int my);
void desktop_icons_stop_dragging_all(void);

// Seçim
void desktop_icons_deselect_all(void);
void desktop_icons_select(int index);
void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2);
void desktop_icons_delete_selected(void);
bool desktop_icons_is_selected(int index);
void desktop_icons_toggle_select(int index);
void desktop_icons_begin_edit(int index);
bool desktop_icons_is_any_editing(void);
void desktop_icons_handle_key(uint16_t scancode, char ascii);

const char* desktop_icons_get_path(int index);
bool desktop_icons_get_rect(int index, int* x, int* y, int* w, int* h);

#endif