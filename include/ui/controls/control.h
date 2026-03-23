#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { int x, y; } ui_point_t;
typedef struct { int w, h; } ui_size_t;

typedef enum {
    UI_EVT_NONE = 0,
    UI_EVT_MOUSE_MOVE,
    UI_EVT_MOUSE_DOWN,
    UI_EVT_MOUSE_UP,
    UI_EVT_CLICK,
    UI_EVT_KEY_DOWN,
    UI_EVT_KEY_UP,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    int mouse_x, mouse_y;
    int mouse_button; // 0: left
    int key;
} ui_event_t;

struct ui_control;
typedef struct ui_control ui_control_t;

typedef struct {
    void (*draw)(ui_control_t* c);
    bool (*handle_event)(ui_control_t* c, const ui_event_t* e); // handled?
} ui_control_vtbl_t;

struct ui_control {
    int id;
    char name[64];

    ui_point_t location;
    ui_size_t  size;

    bool visible;
    bool enabled;

    bool hovered;
    bool pressed;

    ui_control_t* parent;
    ui_control_t* first_child;
    ui_control_t* next_sibling;

    const ui_control_vtbl_t* vtbl;
};

void ui_control_init(ui_control_t* c, int id, ui_point_t loc, ui_size_t size, const ui_control_vtbl_t* vtbl);

// hit-test için rect gerekmesin diye:
// rect_contains fonksiyonu direkt control alanıyla çalışacak
bool ui_control_contains(const ui_control_t* c, int px, int py);

void ui_control_add_child(ui_control_t* parent, ui_control_t* child);

void ui_control_set_name(ui_control_t* c, const char* name);
ui_control_t* ui_find_control_by_name(ui_control_t* root, const char* name);