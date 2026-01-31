// src/lib/app/app_manager.c
#include <app/app_manager.h>
#include <stdint.h>

#include <ui/wm.h>

// demo app factory
app_t* demo_app_create(void);
// terminal app factory
app_t* terminal_app_create(void);

#define APP_MAX 8
static app_t* g_apps[APP_MAX];
static int g_app_count = 0;

void appmgr_init(void) {
    g_app_count = 0;
    for (int i = 0; i < APP_MAX; ++i) g_apps[i] = 0;
}

static void appmgr_add(app_t* a) {
    if (!a) return;
    if (g_app_count >= APP_MAX) return;
    g_apps[g_app_count++] = a;
}

app_t* appmgr_start_demo(void)
{
    app_t* a = demo_app_create();
    if (!a) return 0;

    // demo pencereyi WM'de oluştur
    int win_id = wm_add_window(80, 60, 420, 260, "KuvixOS Demo", a);
    a->win_id = win_id;

    if (a->v && a->v->on_create) a->v->on_create(a);

    appmgr_add(a);
    return a;
}

app_t* appmgr_start_terminal(void)
{
    app_t* a = terminal_app_create();
    if (!a) return 0;

    int win_id = wm_add_window(120, 90, 520, 320, "Terminal", a);
    a->win_id = win_id;

    if (a->v && a->v->on_create) a->v->on_create(a);

    // appmgr_add(a);  (senin appmgr_add internal ise burada ekle)
    return a;
}

app_t* appmgr_get_app_by_window_id(int win_id)
{
    for (int i = 0; i < g_app_count; ++i) {
        if (g_apps[i] && g_apps[i]->win_id == win_id) return g_apps[i];
    }
    return 0;
}
