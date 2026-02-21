#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>   // ✅ app_vtbl_t için

#define NOTEPAD_MAX_TEXT 4096
#define NOTEPAD_MAX_TABS 8

typedef struct {
    char     file_path[128];
    char     text[NOTEPAD_MAX_TEXT];
    uint32_t cursor;
    bool     is_dirty;
} notepad_tab_t;

typedef struct {
    // tabs
    notepad_tab_t tabs[NOTEPAD_MAX_TABS];
    int           tab_count;
    int           active_tab;

    int  window_id;
    bool active;
    bool menu_open;      // Menü açık mı?

    // close prompt state
    bool close_pending;
    bool close_after_save;
    int  pending_close_win_id;

    // ✅ desktop open race fix
    bool pending_open;
    char pending_path[128];
} notepad_t;

void notepad_init(void);
void notepad_draw(void);
void notepad_handle_key(uint8_t scancode);
void notepad_open_file(const char* path);

// ✅ app_manager bunu linkleyebilsin
extern const app_vtbl_t notepad_vtbl;

#endif