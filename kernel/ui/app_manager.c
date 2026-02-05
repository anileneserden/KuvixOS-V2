#include <app/app_manager.h>
#include <app/app.h>
#include <stdint.h>
#include <ui/wm.h>
#include <kernel/printk.h> 
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

// --- 1. DIŞARIDAN GELEN VTABLE TANIMLARI ---
extern const app_vtbl_t terminal_vtbl; 
extern const app_vtbl_t file_manager_vtbl; 
extern const app_vtbl_t notepad_vtbl; // 👈 Notepad eklendi

// --- 2. UYGULAMA TANIMLAMA YAPISI ---
typedef struct {
    int id;
    const char* title;
    const app_vtbl_t* vtbl; 
    int default_x, default_y, default_w, default_h;
    uint32_t data_size; // 👈 Uygulamanın özel verisi (buffer vb.) için gereken boyut
} app_definition_t;

// --- 3. REGISTRY (Kayıt Listesi) ---
static app_definition_t app_registry[] = {
    { 1, "Terminal",         &terminal_vtbl,      120, 90,  520, 320, 1024 }, // Örn: 1KB terminal buffer
    { 2, "File Manager",     &file_manager_vtbl,  40,  60,  420, 260, 2048 }, // Örn: 2KB file list buffer
    { 3, "Notepad",          &notepad_vtbl,       150, 100, 450, 350, 4096 }, // 👈 4KB notepad buffer
    { 0, NULL,               NULL,                0,   0,   0,   0,   0    } 
};

// --- 4. YÖNETİM DEĞİŞKENLERİ ---
#define APP_MAX 16
static app_t* g_apps[APP_MAX];
static int g_app_count = 0;

// --- 5. TEMEL FONKSİYONLAR ---

void appmgr_init(void) {
    g_app_count = 0;
    for (int i = 0; i < APP_MAX; ++i) g_apps[i] = NULL;
    printk("App Manager: Sistem baslatildi.\n");
}

app_t* appmgr_get_app_by_window_id(int win_id) {
    if (win_id == -1) return NULL;
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i] != NULL && g_apps[i]->win_id == win_id) {
            return g_apps[i];
        }
    }
    return NULL;
}

/**
 * Uygulama ID'sine göre yeni bir uygulama başlatır.
 */
app_t* appmgr_start_app(int app_id) {
    app_definition_t* def = NULL;
    for (int i = 0; app_registry[i].id != 0; i++) {
        if (app_registry[i].id == app_id) {
            def = &app_registry[i];
            break;
        }
    }

    if (!def || !def->vtbl) return NULL;

    // 1. App yapısı için yer ayır
    app_t* a = (app_t*)kmalloc(sizeof(app_t));
    if (!a) return NULL;

    // 2. Uygulamanın özel verisi (Notepad buffer vb.) için yer ayır
    if (def->data_size > 0) {
        a->user = kmalloc(def->data_size); // app.h'taki isme göre (user veya private_data)
        if (a->user) {
            memset(a->user, 0, def->data_size);
        }
    } else {
        a->user = NULL;
    }

    a->v = def->vtbl;
    a->visible = 1;
    
    // Pencereyi oluştur ve 'a' nesnesini owner olarak ata
    int win_id = wm_add_window(def->default_x, def->default_y, 
                               def->default_w, def->default_h, 
                               def->title, a); 
    
    a->win_id = win_id;

    // Uygulama özel başlatma fonksiyonunu çağır (Notepad_on_create gibi)
    if (a->v && a->v->on_create) {
        a->v->on_create(a);
    }

    if (g_app_count < APP_MAX) {
        g_apps[g_app_count++] = a;
    }

    wm_set_active(win_id);
    return a;
}

// Kolay başlatma fonksiyonları
app_t* appmgr_start_terminal(void)     { return appmgr_start_app(1); }
app_t* appmgr_start_file_manager(void) { return appmgr_start_app(2); }
app_t* appmgr_start_notepad(void)      { return appmgr_start_app(3); }