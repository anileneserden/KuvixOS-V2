#include <app/app_manager.h>
#include <app/app.h>
#include <stdint.h>
#include <ui/wm.h>
#include <kernel/printk.h> 
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

// --- 1. DIŞARIDAN GELEN VTABLE TANIMLARI ---
// Bu tablolar ilgili .c dosyalarında tanımlı olmalıdır.
extern const app_vtbl_t terminal_vtbl; 
// extern const app_vtbl_t file_manager_app; 

// --- 2. UYGULAMA TANIMLAMA YAPISI ---
typedef struct {
    int id;
    const char* title;
    const app_vtbl_t* vtbl; // Uygulamanın fonksiyon tablosu
    int default_x, default_y, default_w, default_h;
} app_definition_t;

// --- 3. REGISTRY (Kayıt Listesi) ---
// Yeni bir uygulama eklemek istersen sadece bu listeye ekleme yapman yeterli.
static app_definition_t app_registry[] = {
    { 1, "Terminal",         &terminal_vtbl,      120, 90, 520, 320 },
    { 2, "KuvixOS Demo",     NULL,                80, 60, 420, 260 },
    { 0, NULL,               NULL,                0, 0, 0, 0 } 
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

/**
 * Bir Window ID'ye ait uygulama nesnesini bulur.
 * desktop.c içindeki klavye/fare yönlendirmesi için kullanılır.
 */
app_t* appmgr_get_app_by_window_id(int win_id) {
    if (win_id == -1) return NULL;
    
    // Yöntem A: Kendi listemizden tara (Güvenli)
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i] != NULL && g_apps[i]->win_id == win_id) {
            return g_apps[i];
        }
    }
    
    // Yöntem B: WM'den doğrudan çek (Hızlı - wm_get_owner varsa)
    // return (app_t*)wm_get_owner(win_id);
    
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

    if (!def) return NULL;

    app_t* a = (app_t*)kmalloc(sizeof(app_t));
    if (!a) return NULL;

    a->v = def->vtbl;
    a->visible = 1;
    
    // 6. parametre olan 'a' eklendi:
    int win_id = wm_add_window(def->default_x, def->default_y, 
                               def->default_w, def->default_h, 
                               def->title, a); 
    
    a->win_id = win_id;

    // Eğer wm_add_window içinde zaten set_owner yapıyorsan 
    // aşağıdaki satıra gerek kalmayabilir, ama garanti olsun:
    // wm_set_owner(win_id, a); 

    if (a->v && a->v->on_create) {
        a->v->on_create(a);
    }

    if (g_app_count < APP_MAX) {
        g_apps[g_app_count++] = a;
    }

    // wm_set_active fonksiyonunun wm.h içinde tanımlı olduğundan emin ol
    wm_set_active(win_id);

    return a;
}

// Yardımcı kısayollar
app_t* appmgr_start_terminal(void) { return appmgr_start_app(1); }