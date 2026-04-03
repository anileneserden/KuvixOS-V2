#include <app/app_manager.h>
#include <app/app.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>

#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <ui/wm.h>
#include <ui/desktop.h>
#include <kernel/user.h>

#include <ui/apps/notepad.h>
#include <ui/apps/file_manager.h>
#include <ui/apps/kuvix_browser.h>
#include <ui/apps/terminal.h>
#include <ui/apps/pixel_draw_app.h>
#include <ui/apps/demo_font.h>
#include <ui/apps/scroll_demo.h>
#include <ui/apps/kuvix_store.h>
#include <ui/apps/settings.h>
#include <ui/apps/designer.h>
#include <ui/apps/game_engine_window.h>
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
extern const app_vtbl_t game_engine_window_vtbl;
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
    {  1, "Terminal",      &terminal_vtbl,        120,  90, 520, 320, sizeof(terminal_t)        },
    {  2, "File Manager",  &file_manager_vtbl,     40,  60, 420, 260, sizeof(file_mgr_t)        },
    {  3, "Notepad",       &notepad_vtbl,         150, 100, 450, 350, sizeof(notepad_t)         },
    {  4, "Setup Wizard",  &setup_wizard_vtbl,    140,  90, 520, 320, 1024                      },
    {  5, "Demo",          &demo_app_vtbl,        160, 120, 520, 240, 0                         },
    {  6, "Calculator",    &calculator_vtbl,      160, 120, 300, 380, 0                         },
    {  7, "Run",           &run_vtbl,             200, 140, 420, 140, 256                       },
    {  8, "Grid Demo",     &grid_demo_app_vtbl,   120,  80, 720, 540, 16                        },
    {  9, "Pixel Draw",    &pixel_draw_app_vtbl,  120,  80, 900, 650, sizeof(pixel_draw_app_t)  },
    { 10, "Font Demo",     &demo_font_vtbl,       140,  90, 700, 500, sizeof(demo_font_t)       },
    { 11, "Scroll Demo",   &scroll_demo_vtbl,     120,  90, 520, 320, sizeof(scroll_demo_t)     },
    { 12, "KuvixStore",    &kuvix_store_vtbl,     160, 120, 720, 420, sizeof(kuvix_store_t)     },
    { 13, "Settings",      &settings_vtbl,        170, 120, 640, 420, sizeof(settings_t)        },
    { 14, "Designer",      &designer_vtbl,        170, 120, 640, 420, sizeof(designer_t)        },
    { 18, "Game Engine",   &game_engine_window_vtbl, 150, 100, 720, 480, sizeof(game_engine_window_t) },
    { 15, "Kuvix Browser", &kuvix_browser_vtbl,   160, 120, 820, 520, sizeof(kuvix_browser_t)   },
    { 16, "KBI Viewer",    &kbi_viewer_vtbl,      160, 120, 820, 520, sizeof(kbi_viewer_t)      },
    { 17, "Controls test", &controls_test_vtbl,   160, 120, 820, 520, sizeof(controls_test_t)   },
    {  0, NULL,            NULL,                    0,   0,   0,   0, 0                         }
};

// ------------------------------------------------------------
// FILE ASSOCIATIONS (MVP)
// ------------------------------------------------------------

typedef struct {
    const char* ext;            // "html", "txt"
    int handlers[8];            // app id list
    int handler_count;
    int default_handler;        // app id (neg/0 => yok)
} file_assoc_t;

#define APPID_NOTEPAD   3
#define APPID_BROWSER   15

static file_assoc_t g_assoc[] = {
    { "html", { APPID_BROWSER, APPID_NOTEPAD }, 2, APPID_BROWSER },
    { "htm",  { APPID_BROWSER, APPID_NOTEPAD }, 2, APPID_BROWSER },
    { "txt",  { APPID_NOTEPAD },                1, APPID_NOTEPAD },
    { "kth",  { APPID_NOTEPAD },                1, APPID_NOTEPAD },
};

static int g_assoc_count = (int)(sizeof(g_assoc) / sizeof(g_assoc[0]));

// ------------------------------------------------------------
// RUNNING APPS
// ------------------------------------------------------------

// 18 app var, 16 yetmez -> 32 yap
#define APP_MAX 32
static app_t* g_apps[APP_MAX];

// ------------------------------------------------------------
// SMALL UTILS
// ------------------------------------------------------------

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int str_eq_ci(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (to_lower(*a) != to_lower(*b)) return 0;
        a++; b++;
    }
    return (*a == 0 && *b == 0);
}

static bool ends_with(const char* s, const char* suf) {
    if (!s || !suf) return false;
    int sl = (int)strlen(s);
    int pl = (int)strlen(suf);
    if (sl < pl) return false;
    return strcmp(s + (sl - pl), suf) == 0;
}

static const char* path_ext(const char* path) {
    if (!path) return 0;
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return 0;
    return dot + 1; // "html"
}

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

static const char* appmgr_title_from_id(int app_id) {
    app_definition_t* def = appmgr_get_def(app_id);
    return def ? def->title : "Unknown";
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

void appmgr_on_window_closed(int win_id) {
    for (int i = 0; i < APP_MAX; i++) {
        app_t* a = g_apps[i];
        if (!a) continue;
        if (a->win_id != win_id) continue;

        if (a->v && a->v->on_destroy)
            a->v->on_destroy(a);

        printk("[AppMgr] close app_id=%d win=%d user=%p\n", a->id, a->win_id, a->user);

        if (a->user) kfree(a->user);
        kfree(a);
        g_apps[i] = NULL;

        return;
    }
}

// ------------------------------------------------------------
// ENGINE: App instance başlatır
// ------------------------------------------------------------

app_t* appmgr_start_app(int app_id) {
    app_definition_t* def = appmgr_get_def(app_id);
    if (!def || !def->vtbl) return NULL;

    // Notepad singleton örneği
    if (app_id == APPID_NOTEPAD) {
        app_t* existing = appmgr_get_running_by_id(app_id);
        if (existing && wm_is_window_alive(existing->win_id)) {
            wm_set_active(existing->win_id);
            return existing;
        }
    }

    // Run singleton
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

// ------------------------------------------------------------
// FILE ASSOCIATION API (Fileman sağ-tık menüsü burayı çağıracak)
// ------------------------------------------------------------

int appmgr_get_handlers_for_path(const char* path, int* out, int cap, int* out_default) {
    if (out_default) *out_default = -1;
    if (!path || !out || cap <= 0) return 0;

    const char* ext = path_ext(path);
    if (!ext || !ext[0]) return 0;

    for (int i = 0; i < g_assoc_count; i++) {
        if (str_eq_ci(g_assoc[i].ext, ext)) {
            int n = g_assoc[i].handler_count;
            if (n > cap) n = cap;
            for (int k = 0; k < n; k++) out[k] = g_assoc[i].handlers[k];
            if (out_default) *out_default = g_assoc[i].default_handler;
            return n;
        }
    }
    return 0;
}

// ------------------------------------------------------------
// OPEN WITH: seçilen app_id ile path aç
// ------------------------------------------------------------

app_t* appmgr_open_with_appid(int app_id, const char* path) {
    if (!path || !path[0]) return NULL;

    printk("[AppMgr] open_with: app=%d(%s) path='%s'\n", app_id, appmgr_title_from_id(app_id), path);

    // klasör => her zaman File Manager
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1 && st.type == VFS_T_DIR) {
        return appmgr_start_app(2);
    }

    // Notepad => dosyayı aç
    if (app_id == APPID_NOTEPAD) {
        app_t* a = appmgr_start_app(APPID_NOTEPAD);
        if (a) notepad_open_file(path);
        return a;
    }

    // Browser => file: URL ile aç (html değilse bile şimdilik açabilir)
    if (app_id == APPID_BROWSER) {
        char url[VFS_PATH_MAX + 8];
        url[0] = 0;
        strncpy(url, "file:", sizeof(url) - 1);
        url[sizeof(url) - 1] = 0;
        strncat(url, path, sizeof(url) - (size_t)strlen(url) - 1);

        app_t* bapp = appmgr_start_app(APPID_BROWSER);
        if (bapp) {
            kuvix_browser_open_url(bapp, url);
            return bapp;
        }
        return NULL;
    }

    // Diğer app’ler için şimdilik sadece başlat
    // (ileride "open_file" callback standardı ekleriz)
    return appmgr_start_app(app_id);
}

// ------------------------------------------------------------
// DEFAULT OPEN: çift tık / "Aç"
// ------------------------------------------------------------

app_t* appmgr_open_path(const char* path) {
    if (!path || !path[0]) return NULL;

    printk("[AppMgr] open_path: '%s'\n", path);

    // klasör -> File Manager
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1 && st.type == VFS_T_DIR) {
        return appmgr_start_app(2);
    }

    // ksf shortcut
    if (ends_with(path, ".ksf")) {
        uint8_t buf[256];
        uint32_t sz = 0;

        if (vfs_read_all(path, buf, sizeof(buf) - 1, &sz) == 1) {
            buf[sz] = 0;

            char* p = strstr((char*)buf, "app_id=");
            if (p) {
                int id = 0;
                p += 7;
                while (*p >= '0' && *p <= '9') { id = id * 10 + (*p - '0'); p++; }
                if (id > 0) return appmgr_start_app(id);
            }
        }
        printk("[AppMgr] ksf read FAILED or invalid: %s\n", path);
        return NULL;
    }

    // ✅ association ile default handler
    int handlers[8];
    int def = -1;
    int n = appmgr_get_handlers_for_path(path, handlers, 8, &def);
    if (n > 0 && def > 0) {
        return appmgr_open_with_appid(def, path);
    }

    // fallback: önceki davranış (istersen kaldırabilirsin)
    if (ends_with(path, ".txt") || ends_with(path, ".kth")) {
        return appmgr_open_with_appid(APPID_NOTEPAD, path);
    }
    if (ends_with(path, ".html") || ends_with(path, ".htm")) {
        return appmgr_open_with_appid(APPID_BROWSER, path);
    }

    printk("AppManager: bilinmeyen path (default app yok): %s\n", path);
    return NULL;
}

// ------------------------------------------------------------

void appmgr_update_all(void) {
    for (int i = 0; i < APP_MAX; i++) {
        app_t* a = g_apps[i];
        const ui_window_t* w;

        if (!a) continue;
        if (!a->visible) continue;
        if (!wm_is_window_alive(a->win_id)) continue;
        if (a->v && a->v->on_update) a->v->on_update(a);

        if (!a->wants_continuous_redraw) continue;

        w = wm_get_window_ptr(a->win_id);
        if (!w) {
            desktop_request_redraw();
            continue;
        }

        if (w->state == WIN_MINIMIZED) continue;

        desktop_damage_rect(w->x, w->y, w->w, w->h);
    }
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