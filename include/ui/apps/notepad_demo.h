#ifndef NOTEPAD_DEMO_H
#define NOTEPAD_DEMO_H

#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>   // ✅ app_vtbl_t

typedef struct {
    int  window_id;
    bool active;

    // menu state
    bool menu_open;

    // editor focus (demo)
    bool editor_focus;

    // status bar visibility
    bool status_visible;

    // caret blink (same style as notepad)
    uint32_t caret_last_ms;
    uint32_t caret_blink_ms;
    int      caret_visible;
} notepad_demo_t;

// ✅ app_manager / registry bunu linkleyebilsin
extern const app_vtbl_t notepad_demo_vtbl;

#endif