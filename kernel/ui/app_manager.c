#include <app/app_manager.h>
#include <app/app.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <stdint.h>
#include <ui/wm.h>
#include <ui/desktop.h>

#include <ui/apps/notepad.h>
#include <ui/apps/pixel_draw_app.h>
#include <ui/apps/demo_font.h>
#include <ui/apps/file_manager.h>
#include <ui/apps/terminal.h>
#include <ui/apps/scroll_demo.h>
#include <ui/apps/kuvix_store.h>
#include <ui/apps/settings.h>
#include <ui/apps/designer.h>
#include <ui/apps/kuvix_browser.h>
#include <ui/apps/kbi_viewer.h>
#include <ui/apps/controls_test.h>

// --- DIŞARIDAN GELEN VTABLE'LER ---
extern const app_vtbl_t terminal_vtbl;
extern const app_vtbl_t file_manager_vtbl;
extern const app_vtbl_t notepad_vtbl;
extern const app_vtbl_t setup_wizard_vtbl;
extern const app_vtbl_t demo_app_vtbl;
extern const app_vtbl_t calculator_vtbl;
extern const app_vtbl_t run_vtbl;
extern const app_vtbl_t grid_demo_app_vtbl;
extern const app_vtbl_t pixel_draw_app_vtbl;
extern const app_vtbl_t demo_font_vtbl;
extern const app_vtbl_t scroll_demo_vtbl;
extern const app_vtbl_t kuvix_store_vtbl;
extern const app_vtbl_t settings_vtbl;
extern const app_vtbl_t designer_vtbl;
extern const app_vtbl_t kuvix_browser_vtbl;
extern const app_vtbl_t kbi_viewer_vtbl;
extern const app_vtbl_t controls_test_vtbl;

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
    {  1, "Terminal",      &terminal_vtbl,       120,  90, 520, 320, sizeof(terminal_t)       },
    {  2, "File Manager",  &file_manager_vtbl,    40,  60, 420, 260, sizeof(file_mgr_t)       },
    {  3, "Notepad",       &notepad_vtbl,        150, 100, 450, 350, sizeof(notepad_t)        },
    {  4, "Setup Wizard",  &setup_wizard_vtbl,   140,  90, 520, 320,                     1024 },
    {  5, "Demo",          &demo_app_vtbl,       160, 120, 520, 240,                        0 },
    {  6, "Calculator",    &calculator_vtbl,     160, 120, 300, 380,                        0 },
    {  7, "Run",           &run_vtbl,            200, 140, 420, 140,                      256 },
    {  8, "Grid Demo",     &grid_demo_app_vtbl,  120,  80, 720, 540,                       16 },
    {  9, "Pixel Draw",    &pixel_draw_app_vtbl, 120,  80, 900, 650, sizeof(pixel_draw_app_t) },
    { 10, "Font Demo",     &demo_font_vtbl,      140,  90, 700, 500, sizeof(demo_font_t)      },
    { 11, "Scroll Demo",   &scroll_demo_vtbl,    120,  90, 520, 320, sizeof(scroll_demo_t)    },
    { 12, "KuvixStore",    &kuvix_store_vtbl,    160, 120, 720, 420, sizeof(kuvix_store_t)    },
    { 13, "Settings",      &settings_vtbl,       170, 120, 640, 420, sizeof(settings_t)       },
    { 14, "Designer",      &designer_vtbl,       170, 120, 640, 420, sizeof(designer_t)       },
    { 15, "Kuvix Browser", &kuvix_browser_vtbl,  160, 120, 820, 520, sizeof(kuvix_browser_t)  },
    { 16, "KBI Viewer",    &kbi_viewer_vtbl,     160, 120, 820, 520, sizeof(kbi_viewer_t)     },
    { 17, "Controls test", &controls_test_vtbl,  160, 120, 820, 520, sizeof(controls_test_t)  },
    { 0,  NULL,                        NULL,       0,   0,   0,   0,                       0  }
};

#define APP_MAX 16
static app_t* g_apps[APP_MAX];

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
    for (int i = 0; i < APP_MAX; i++) {
        if (g_apps[i] && g_apps[i]->id == app_id && g_apps[i]->visible)
            return g_apps[i];
    }
    return NULL;
}

static int appmgr_find_free_slot(void) {
    for (int i = 0; i < APP_MAX; i++) {
        if (g_apps[i] == NULL) return i;
    }
    return -1;    
}

void appmgr_on_window_closed(int win_id) {
    for (int i = 0; i < APP_MAX; i++) {
        app_t* a = g_apps[i];
        if (!a) continue;
        if (a->win_id != win_id) continue;

        // app destroy callback
        if (a->v && a->v->on_destroy)
            a->v->on_destroy(a);
        
        printk("[AppMgr] close app_id=%d win=%d user=%p\n", a->id, a->win_id, a->user);

        // user data free
        if (a->user) kfree(a->user);

        // app free
        kfree(a);
        g_apps[i] = NULL;

        // aktif pencere kapandıysa WM aktifliği temizlemek isteyebilirsin
        // (WM tarafında zaten handle yapıyorsan sorun yok)
        return;
    }
    
}

// ------------------------------------------------------------
// PUBLIC API
// ------------------------------------------------------------

void appmgr_init(void) {
    for (int i = 0; i < APP_MAX; i++)
        g_apps[i] = NULL;

    printk("AppManager: baslatildi.\n");
}

app_t* appmgr_get_app_by_window_id(int win_id) {
    for (int i = 0; i < APP_MAX; i++) {
        if (g_apps[i] && g_apps[i]->win_id == win_id)
            return g_apps[i];
    }
    return NULL;
}

int appmgr_find_window_by_app_id(int app_id) {
    for (int i = 0; i < APP_MAX; i++) {
        if (g_apps[i] && g_apps[i]->id == app_id && g_apps[i]->visible) {
            if (wm_is_window_alive(g_apps[i]->win_id))
                return g_apps[i]->win_id;
        }
    }
    return -1;
}

// ------------------------------------------------------------
// ENGINE: App instance başlatır
// ------------------------------------------------------------

app_t* appmgr_start_app(int app_id) {

    app_definition_t* def = appmgr_get_def(app_id);
    if (!def || !def->vtbl) return NULL;

    // örnek Notepad singleton yapmak istersen
    if (app_id == 3) {
        app_t* existing = appmgr_get_running_by_id(app_id);
        if (existing && wm_is_window_alive(existing->win_id)) {
            wm_set_active(existing->win_id);
            return existing;
        }
    }

    // Run'ı da singleton yap (Win+R spam yemesin)
    if (app_id == 7) {
        int w = appmgr_find_window_by_app_id(7);
        if (w != -1) {
            wm_set_active(w);
            return appmgr_get_app_by_window_id(w);
        }
    }
    
    int slot = appmgr_find_free_slot();
    if (slot < 0) {
        printk("AppManager: limit dolu\n");
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
        if (!a->user) {
            printk("[AppMgr] ERROR: kmalloc failed for app_id=%d size=%u\n", app_id, def->data_size);
            kfree(a);
            return NULL;
        }
        memset(a->user, 0, def->data_size);
    }

    int win_id = wm_add_window(def->default_x, def->default_y,
                               def->default_w, def->default_h,
                               def->title, a);

    a->win_id = win_id;

    if (a->v && a->v->on_create)
        a->v->on_create(a);

    g_apps[slot] = a;

    wm_set_active(win_id);

    desktop_invalidate_full();

    printk("[AppMgr] start app_id=%d title=%s -> win_id=%d user=%p\n",
       a->id, def->title, win_id, a->user);

    return a;
}

static bool ends_with(const char* s, const char* suf) {
    if (!s || !suf) return false;
    int sl = (int)strlen(s);
    int pl = (int)strlen(suf);
    if (sl < pl) return false;
    return strcmp(s + (sl - pl), suf) == 0;
}

app_t* appmgr_open_path(const char* path) {
    if (!path || !path[0]) return NULL;

    printk("[AppMgr] open_path: '%s'\n", path);

    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1) {
        // Klasör -> File Manager
        if (st.type == VFS_T_DIR) {
            return appmgr_start_app(2);
        }
    }

    // .txt -> Notepad (DOSYAYI AÇ)
    if (ends_with(path, ".txt") || ends_with(path, ".kth")) {
        app_t* a = appmgr_start_app(3);
        notepad_open_file(path);
        return a;
    }

    // .ksf -> shortcut parse
    if (ends_with(path, ".ksf")) {
        uint8_t buf[256];
        uint32_t sz = 0;

        if (vfs_read_all(path, buf, sizeof(buf) - 1, &sz) == 1) {
            buf[sz] = 0;

            printk("[AppMgr] ksf read ok: %u bytes\n", sz);
            printk("[AppMgr] ksf content:\n%s\n", (char*)buf);

            char* p = strstr((char*)buf, "app_id=");
            printk("[AppMgr] find app_id=: %p\n", (void*)p);

            if (p) {
                int id = 0;
                p += 7;
                while (*p >= '0' && *p <= '9') { id = id * 10 + (*p - '0'); p++; }
                printk("[AppMgr] parsed id=%d\n", id);

                if (id > 0) return appmgr_start_app(id);
            }
        } else {
            printk("[AppMgr] ksf read FAILED: %s\n", path);
        }
    }

    printk("AppManager: bilinmeyen path: %s\n", path);
    return NULL;
}

bool appmgr_any_continuous_redraw(void) {
    for (int i = 0; i < APP_MAX; i++) {
        app_t* a = g_apps[i];
        if (!a) continue;
        if (!a->visible) continue;
        if (!wm_is_window_alive(a->win_id)) continue;

        if (a->wants_continuous_redraw) return true;
    }
    return false;
}