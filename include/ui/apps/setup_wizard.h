#ifndef SETUP_WIZARD_H
#define SETUP_WIZARD_H

#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>

// Wizard step enum
typedef enum {
    WZ_WELCOME = 0,
    WZ_TARGET,
    WZ_LICENSE,
    WZ_PROGRESS,
    WZ_DONE
} wizard_step_t;

// Wizard state (app_manager user)
typedef struct {
    int window_id;
    wizard_step_t step;

    char target_path[128];
    bool license_accepted;

    int progress;   // 0..100
    int tick;       // fake timer
    bool did_install;
    bool add_desktop_icon;   // ✅ Masaüstüne ikon ekle
} setup_wizard_t;

// VTABLE export
extern const app_vtbl_t setup_wizard_vtbl;

#endif
