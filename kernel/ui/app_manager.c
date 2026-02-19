#include <app/app_manager.h>
#include <app/app.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <stdint.h>
#include <ui/wm.h>

// --- DIŞARIDAN GELEN VTABLE'LER ---
extern const app_vtbl_t terminal_vtbl;
extern const app_vtbl_t file_manager_vtbl;
extern const app_vtbl_t notepad_vtbl;
extern const app_vtbl_t setup_wizard_vtbl;
extern const app_vtbl_t demo_app_vtbl;
extern const app_vtbl_t calculator_vtbl;

// ------------------------------------------------------------
// APP REGISTRY (Engine katmanı)
// ------------------------------------------------------------

typedef struct {
    int id;
    const char* title;
    const app_vtbl_t* vtbl;
    int default_x, default_y, default_w, default_h;
    uint32_t data_size;
} app_definition_t;

static app_definition_t app_registry[] = {
    { 1, "Terminal",     &terminal_vtbl,      120,  90, 520, 320, 1024 },
    { 2, "File Manager", &file_manager_vtbl,   40,  60, 420, 260, 2048 },
    { 3, "Notepad",      &notepad_vtbl,       150, 100, 450, 350, 1024 },
    { 4, "Setup Wizard", &setup_wizard_vtbl,  140,  90, 520, 320, 1024 },
    { 5, "Demo",         &demo_app_vtbl,      160, 120, 520, 240,    0 },
    { 6, "Calculator",   &calculator_vtbl,    160, 120, 520, 240,    0 },
    { 0, NULL,           NULL,                  0,   0,   0,   0,    0 }
};

#define APP_MAX 16
static app_t* g_apps[APP_MAX];
static int g_app_count = 0;

// ------------------------------------------------------------
// INTERNAL HELPERS
// ------------------------------------------------------------

static app_definition_t* appmgr_get_def(int app_id) {
    for (int i = 0; app_registry[i].id != 0; i++) {
        if (app_registry[i].id == app_id)
            return &app_registry[i];
    }
    return NULL;
}

static app_t* appmgr_get_running_by_id(int app_id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i] && g_apps[i]->id == app_id && g_apps[i]->visible)
            return g_apps[i];
    }
    return NULL;
}

// ------------------------------------------------------------
// PUBLIC API
// ------------------------------------------------------------

void appmgr_init(void) {
    g_app_count = 0;
    for (int i = 0; i < APP_MAX; i++)
        g_apps[i] = NULL;

    printk("AppManager: baslatildi.\n");
}

app_t* appmgr_get_app_by_window_id(int win_id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i] && g_apps[i]->win_id == win_id)
            return g_apps[i];
    }
    return NULL;
}

// ------------------------------------------------------------
// ENGINE: App instance başlatır
// ------------------------------------------------------------

app_t* appmgr_start_app(int app_id) {

    app_definition_t* def = appmgr_get_def(app_id);
    if (!def || !def->vtbl) return NULL;

    // örnek: Notepad singleton yapmak istersen
    if (app_id == 3) {
        app_t* existing = appmgr_get_running_by_id(app_id);
        if (existing && wm_is_window_alive(existing->win_id)) {
            wm_set_active(existing->win_id);
            return existing;
        }
    }

    if (g_app_count >= APP_MAX) {
        printk("AppManager: limit dolu!\n");
        return NULL;
    }

    app_t* a = (app_t*)kmalloc(sizeof(app_t));
    if (!a) return NULL;
    memset(a, 0, sizeof(app_t));

    a->id = def->id;
    a->v  = def->vtbl;
    a->visible = 1;

    if (def->data_size > 0) {
        a->user = kmalloc(def->data_size);
        if (a->user)
            memset(a->user, 0, def->data_size);
    }

    int win_id = wm_add_window(def->default_x, def->default_y,
                               def->default_w, def->default_h,
                               def->title, a);

    a->win_id = win_id;

    if (a->v && a->v->on_create)
        a->v->on_create(a);

    g_apps[g_app_count++] = a;

    wm_set_active(win_id);

    return a;
}

// ------------------------------------------------------------
// ROUTER: Path'e göre doğru app'i açar
// ------------------------------------------------------------

app_t* appmgr_open_path(const char* path) {

    if (!path) return NULL;

    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1) {

        // Klasör -> File Manager
        if (st.type == VFS_T_DIR) {
            return appmgr_start_app(2);
        }
    }

    // .txt -> Notepad
    if (strstr(path, ".txt")) {
        return appmgr_start_app(3);
    }

    // .ksf -> shortcut parse
    if (strstr(path, ".ksf")) {

        uint8_t buf[256];
        uint32_t sz = 0;

        if (vfs_read_all(path, buf, sizeof(buf)-1, &sz) >= 0) {
            buf[sz] = 0;

            char* s = (char*)buf;
            char* p = strstr(s, "app_id=");
            if (p) {
                int id = 0;
                p += 7;
                while (*p >= '0' && *p <= '9') {
                    id = id * 10 + (*p - '0');
                    p++;
                }

                if (id > 0)
                    return appmgr_start_app(id);
            }
        }
    }

    printk("AppManager: bilinmeyen path: %s\n", path);
    return NULL;
}
