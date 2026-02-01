#include <app/app_manager.h>
#include <stdint.h>
#include <ui/wm.h>
#include <kernel/printk.h> 
#include <kernel/memory/kmalloc.h>

// 1. ÖNCE YAPI TANIMI (Struct definition)
// Derleyicinin alttaki diziyi ve fonksiyonları anlaması için en üstte olmalı.
typedef struct {
    int id;
    const char* title;
    app_t* (*create_fn)(void);
    int default_x, default_y, default_w, default_h;
} app_definition_t;

// 2. FONKSİYON GÖVDELERİ
// app_manager.h içindeki app_t yapısıyla uyumlu dönüş tipleri.
app_t* terminal_app_create(void) {
    app_t* app = (app_t*)kmalloc(sizeof(app_t));
    if(app) {
        app->v = 0; 
        printk("Terminal app created\n");
    }
    return app;
}

app_t* demo_app_create(void) {
    app_t* app = (app_t*)kmalloc(sizeof(app_t));
    if(app) {
        app->v = 0;
        printk("Demo app created\n");
    }
    return app;
}

// 3. KAYIT LİSTESİ (Registry)
static app_definition_t app_registry[] = {
    { 1, "Terminal",     terminal_app_create, 120, 90, 520, 320 },
    { 2, "KuvixOS Demo", demo_app_create,     80,  60, 420, 260 },
    { 0, 0, 0, 0, 0, 0, 0 } // Liste sonu işareti
};

// 4. UYGULAMA YÖNETİMİ DEĞİŞKENLERİ
#define APP_MAX 8
static app_t* g_apps[APP_MAX];
static int g_app_count = 0;

// 5. FONKSİYONLARIN GERÇEKLENMESİ (Implementation)

void appmgr_init(void) {
    g_app_count = 0;
    for (int i = 0; i < APP_MAX; ++i) g_apps[i] = 0;
}

static void appmgr_add(app_t* a) {
    if (!a || g_app_count >= APP_MAX) return;
    g_apps[g_app_count++] = a;
}

app_t* appmgr_start_app(int app_id) {
    app_definition_t* def = 0;

    // Registry içinde ID'yi ara
    for (int i = 0; app_registry[i].id != 0; i++) {
        if (app_registry[i].id == app_id) {
            def = &app_registry[i];
            break;
        }
    }

    if (!def) return 0; // ID bulunamadı

    // 1. Uygulama objesini yarat
    app_t* a = def->create_fn();
    if (!a) return 0;

    // 2. Pencereyi oluştur
    int win_id = wm_add_window(def->default_x, def->default_y, 
                               def->default_w, def->default_h, 
                               def->title, a);
    a->win_id = win_id;

    // 3. Callback'i tetikle
    if (a->v && a->v->on_create) a->v->on_create(a);

    // 4. Listeye ekle
    appmgr_add(a);
    
    return a;
}

app_t* appmgr_start_terminal(void) { return appmgr_start_app(1); }
app_t* appmgr_start_demo(void)     { return appmgr_start_app(2); }

app_t* appmgr_get_app_by_window_id(int win_id) {
    for (int i = 0; i < g_app_count; ++i) {
        if (g_apps[i] && g_apps[i]->win_id == win_id) return g_apps[i];
    }
    return 0;
}