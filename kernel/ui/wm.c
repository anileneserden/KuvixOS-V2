#include <stdint.h>
#include <ui/wm.h>
#include <ui/window/window.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <app/app.h>

#define WM_MAX_WINDOWS 20

typedef struct {
    ui_window_t win;
    app_t* owner;
} wm_entry_t;

static wm_entry_t g_wins[WM_MAX_WINDOWS];
static int g_count = 0;
static int g_active = -1;

static int g_dragging = 0;
static int g_drag_idx = -1;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;
static int g_z[WM_MAX_WINDOWS];
static int g_drag_restore_pending = 0;

static uint32_t last_click_time = 0;
static int last_click_win = -1;
static int last_click_x = 0;
static int last_click_y = 0;

static int      dc_pending = 0;
static uint32_t dc_time = 0;
static int      dc_win = -1;

extern uint32_t g_ticks_ms;

#define DRAG_THRESHOLD 3
#define DOUBLE_CLICK_MS 350
#define DOUBLE_CLICK_DIST 3

static int g_mouse_down = 0;
static int g_down_x = 0;
static int g_down_y = 0;
static int g_mouse_x = 0;
static int g_mouse_y = 0;

static int g_resizing = 0;
static int g_resize_idx = -1;
static int g_resize_start_mx = 0, g_resize_start_my = 0;
static int g_resize_start_w = 0, g_resize_start_h = 0;

#define RESIZE_GRIP 10
#define WIN_MIN_W   80
#define WIN_MIN_H   60

#ifndef NULL
    #define NULL ((void*)0)
#endif

static const int WM_BORDER  = 2;
static const int WM_TITLE_H = 24;
static const ui_padding_t WM_PAD = { .left=0, .top=0, .right=0, .bottom=0 };

static int dc_x = 0;
static int dc_y = 0;

// --- Dışarıdan Gelen Fonksiyon ve Instance ---
extern void terminal_on_draw_direct(void);
extern app_t terminal_app_instance;

static int contains(const ui_window_t* w, int px, int py) {
    return (px >= w->x && px < w->x + w->w && py >= w->y && py < w->y + w->h);
}

static int in_titlebar(const ui_window_t* w, int px, int py) {
    return contains(w, px, py) && (py >= w->y && py < w->y + WM_TITLE_H);
}

static int in_close_btn(const ui_window_t* w, int mx, int my) {
    const int btn_size = 16;
    const int btn_y = w->y + 4;
    const int btn_x = w->x + w->w - btn_size - 4;
    return (mx >= btn_x && mx < btn_x + btn_size && my >= btn_y && my < btn_y + btn_size);
}

static int in_max_btn(const ui_window_t* w, int mx, int my) {
    const int btn_size = 16;
    const int btn_y = w->y + 4;
    const int close_x = w->x + w->w - btn_size - 4;
    const int x = close_x - btn_size - 4;
    return (mx >= x && mx < x + btn_size && my >= btn_y && my < btn_y + btn_size);
}

static int in_min_btn(const ui_window_t* w, int mx, int my) {
    const int btn_size = 16;
    const int btn_y = w->y + 4;
    const int close_x = w->x + w->w - btn_size - 4;
    const int max_x   = close_x - btn_size - 4;
    const int x       = max_x   - btn_size - 4;
    return (mx >= x && mx < x + btn_size && my >= btn_y && my < btn_y + btn_size);
}

static int in_resize_grip(const ui_window_t* w, int px, int py) {
    int gx = w->x + w->w - RESIZE_GRIP;
    int gy = w->y + w->h - RESIZE_GRIP;
    return (px >= gx && px < w->x + w->w && py >= gy && py < w->y + w->h);
}

static int pick_top(int mx, int my) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
        if (contains(&g_wins[id].win, mx, my)) return id;
    }
    return -1;
}

static void bring_to_front(int win_id) {
    if (win_id < 0 || win_id >= g_count) return;
    int pos = -1;
    for (int i = 0; i < g_count; ++i) {
        if (g_z[i] == win_id) { pos = i; break; }
    }
    if (pos < 0) return;
    int tmp = g_z[pos];
    for (int i = pos; i < g_count - 1; ++i) g_z[i] = g_z[i + 1];
    g_z[g_count - 1] = tmp;
    g_active = win_id;
}

void wm_init(void) {
    g_count = 0;
    g_active = -1;
    g_dragging = 0;
    g_drag_idx = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; ++i) g_z[i] = 0;
}

int wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner) {
    if (g_count < 0 || g_count >= WM_MAX_WINDOWS) return -1;
    int id = g_count;
    const char* safe_title = (title == NULL) ? "Window" : title;
    g_wins[id].win.x = x;
    g_wins[id].win.y = y;
    g_wins[id].win.w = w;
    g_wins[id].win.h = h;
    g_wins[id].win.title = safe_title;
    g_wins[id].win.state = 0;
    g_wins[id].owner = owner;
    g_z[id] = id;
    g_active = id;
    g_count++;
    return id;
}

void wm_close_window(int idx) {
    if (idx < 0 || idx >= g_count) return;
    for (int i = idx; i < g_count - 1; i++) g_wins[i] = g_wins[i + 1];
    g_count--;
    for (int i = 0; i < g_count + 1; i++) {
        if (g_z[i] == idx) {
            for (int j = i; j < g_count; j++) g_z[j] = g_z[j + 1];
            break;
        }
    }
    for (int i = 0; i < g_count; i++) { if (g_z[i] > idx) g_z[i]--; }
    if (g_count == 0) g_active = -1;
    else g_active = g_z[g_count - 1];
    g_mouse_down = 0; g_dragging = 0; g_drag_idx = -1;
}

// Pencerenin sahibini (app_t) belirlemek için kullanılır
void wm_set_owner(int win_id, void* owner_app) {
    if (win_id >= 0 && win_id < g_count) {
        g_wins[win_id].owner = (app_t*)owner_app;
    }
}

// Pencereyi odaklamak (öne getirmek) için kullanılır
void wm_set_active(int win_id) {
    if (win_id >= 0 && win_id < g_count) {
        bring_to_front(win_id);
    }
}

// app_manager.c'nin owner bilgisini çekebilmesi için gerekebilir
void* wm_get_owner(int win_id) {
    if (win_id >= 0 && win_id < g_count) {
        return g_wins[win_id].owner;
    }
    return NULL;
}

int wm_find_window_at(int x, int y) {
    // En üstteki pencereden (g_z[g_count-1]) en alttakine doğru kontrol et
    for (int zi = g_count - 1; zi >= 0; zi--) {
        int id = g_z[zi];
        ui_window_t* w = &g_wins[id].win;
        
        // Eğer koordinatlar bu pencerenin içindeyse ID'yi döndür
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h) {
            return id;
        }
    }
    return -1; // Hiçbir pencereye isabet etmedi (Masaüstü)
}

int wm_get_window(int win_id, ui_window_t* out) {
    if (!out || win_id < 0 || win_id >= g_count) return 0;
    *out = g_wins[win_id].win;
    return 1;
}

static ui_rect_t g_draw_client_rect;
const ui_rect_t* wm_get_draw_client_rect(void) { return &g_draw_client_rect; }

void wm_set_title(int win_id, const char* title) {
    if (win_id >= 0 && win_id < g_count) g_wins[win_id].win.title = title;
}

int wm_get_active_id(void) { return g_active; }
app_t* wm_get_active_owner(void) {
    if (g_active < 0 || g_active >= g_count) return 0;
    return g_wins[g_active].owner;
}

void wm_handle_mouse(int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    g_mouse_x = mx; g_mouse_y = my;
    uint32_t now = g_ticks_ms;
    if (dc_pending && (now - dc_time) > DOUBLE_CLICK_MS) dc_pending = 0;

    if (pressed & 0x01) {
        int idx = pick_top(mx, my);
        if (idx != -1) {
            bring_to_front(idx);
            ui_window_t* w = &g_wins[idx].win;

            // 1. KAPAT BUTONU: Pencereyi yok et
            if (in_close_btn(w, mx, my)) {
                wm_close_window(idx);
                return; // Pencere kapandı, devam etmeye gerek yok
            }

            // 2. MAXIMIZE BUTONU: Tam ekran yap veya eski haline döndür
            if (in_max_btn(w, mx, my)) {
                wm_toggle_maximize(idx);
                return;
            }

            // 3. SÜRÜKLEME: Sadece başlık çubuğuna tıklandıysa (butonlar hariç)
            if (in_titlebar(w, mx, my)) {
                g_mouse_down = 1;
                g_drag_idx = idx;
                g_down_x = mx;
                g_down_y = my;
            }
        }
    }
    if (released & 0x01) { g_mouse_down = 0; g_dragging = 0; g_resizing = 0; }
}

void wm_handle_mouse_move(int mx, int my) {
    g_mouse_x = mx;
    g_mouse_y = my;

    if (g_mouse_down && g_drag_idx != -1) {
        ui_window_t* w = &g_wins[g_drag_idx].win;
        
        // Pencere maksimize edilmişse sürüklenemez
        if (w->state == 1) return;

        // Aradaki farkı pencere konumuna ekle
        int dx = mx - g_down_x;
        int dy = my - g_down_y;

        w->x += dx;
        w->y += dy;

        // Başlangıç noktasını güncelle
        g_down_x = mx;
        g_down_y = my;
        
        g_dragging = 1;
    }
}

ui_rect_t wm_get_client_rect(int win_id) {
    ui_window_t* w = &g_wins[win_id].win;
    ui_rect_t r;
    r.x = w->x + 2;      // Kenarlık payı
    r.y = w->y + 26;     // Başlık çubuğu payı
    r.w = w->w - 4;
    r.h = w->h - 28;
    return r;
}

void wm_toggle_maximize(int idx) {
    ui_window_t* w = &g_wins[idx].win;
    if (w->state == 1) { // MAXIMIZED
        w->x = w->prev_x; w->y = w->prev_y; w->w = w->prev_w; w->h = w->prev_h; w->state = 0;
    } else {
        w->prev_x = w->x; w->prev_y = w->y; w->prev_w = w->w; w->h = w->prev_h;
        w->x = 0; w->y = 24; w->w = fb_get_width(); w->h = fb_get_height() - 24; w->state = 1;
    }
}

void wm_draw(void) {
    for (int zi = 0; zi < g_count; ++zi) {
        int id = g_z[zi];
        ui_window_t* win = &g_wins[id].win;
        app_t* app = g_wins[id].owner;

        if (app && app->visible) {
            // 1. Pencere çerçevesini çiz (Başlık çubuğu vs.)
            ui_window_draw(win, (id == g_active), g_mouse_x, g_mouse_y);

            // 2. KRİTİK: Uygulamanın çizim fonksiyonunu çağır
            if (app->v && app->v->on_draw) {
                app->v->on_draw(app); 
            }
        }
    }
}

// wm.c içine uygun bir yere ekle
void wm_invalidate_window(int win_id) {
    // Şimdilik sadece win_id kontrolü yapıyoruz. 
    // Gelişmiş sistemlerde sadece bu pencerenin olduğu alan "dirty" (kirli) işaretlenir.
    if (win_id >= 0 && win_id < g_count) {
        // Ekranın bir sonraki döngüde çizilmesi gerektiğini belirtir.
        // fb_present() zaten ana döngünde olduğu için bu genelde otomatik olur.
    }
}

// kernel/ui/wm.c
int wm_is_any_window_captured(void) {
    // Eğer bir pencere sürükleniyorsa veya sol tık basılıysa 
    // Masaüstü'nün ikon seçme/sürükleme mantığı çalışmamalı.
    return (g_dragging || g_mouse_down || g_resizing);
}

int wm_dbg_dc_pending(void) { return dc_pending; }
uint32_t wm_dbg_dc_age_ms(uint32_t now_ms) { return dc_pending ? (now_ms - dc_time) : 0; }
int wm_dbg_dc_win(void) { return dc_win; }