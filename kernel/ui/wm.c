#include <stdint.h>
#include <ui/wm.h>
#include <ui/window/window.h>
#include <ui/wm/hittest.h> 
#include <ui/window_chrome.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <app/app.h>
#include <ui/dialogs/save_dialog.h>

#define WM_MAX_WINDOWS 20

typedef struct {
    ui_window_t win;
    app_t* owner;
} wm_entry_t;

static wm_entry_t g_wins[WM_MAX_WINDOWS];
static int g_count = 0;
static int g_active = -1;
static int g_z[WM_MAX_WINDOWS];

static int g_mouse_down = 0;
static int g_dragging = 0;
static int g_drag_idx = -1;
static int g_down_x = 0;
static int g_down_y = 0;
static int g_mouse_x = 0;
static int g_mouse_y = 0;

extern uint32_t g_ticks_ms;

// --- YARDIMCI FONKSİYONLAR ---

static int pick_top(int mx, int my) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
        ui_window_t* w = &g_wins[id].win;
        // Düzeltme: py -> my yapıldı
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) {
            return id;
        }
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

// --- CORE WM FONKSİYONLARI ---

void wm_init(void) {
    g_count = 0;
    g_active = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; ++i) g_z[i] = 0;
}

int wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner) {
    if (g_count >= WM_MAX_WINDOWS) return -1;
    int id = g_count;
    g_wins[id].win.x = x; g_wins[id].win.y = y;
    g_wins[id].win.w = w; g_wins[id].win.h = h;
    
    // BUNLARI EKLE: İlk değerleri koru
    g_wins[id].win.prev_x = x; 
    g_wins[id].win.prev_y = y;
    g_wins[id].win.prev_w = w;
    g_wins[id].win.prev_h = h;

    g_wins[id].win.title = title ? title : "Window";
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
    g_active = (g_count == 0) ? -1 : g_z[g_count - 1];
    g_mouse_down = 0; g_dragging = 0; g_drag_idx = -1;
}

void wm_handle_mouse(int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    g_mouse_x = mx; g_mouse_y = my;

    // 1. ÖNCE SAVE DIALOG KONTROLÜ (Modal olduğu için en öncelikli)
    if (save_dialog_is_active()) {
        // save_dialog_handle_mouse fonksiyonuna koordinatları ve tıklama bilgisini gönder
        save_dialog_handle_mouse(mx, my, (pressed & 0x01));
        
        // Eğer diyalog aktifse, tıklamanın pencerelere (alt katmana) geçmesini engelle
        return; 
    }

    // 2. NORMAL PENCERE KONTROLLERİ (Eğer diyalog açık değilse burası çalışır)
    if (pressed & 0x01) {
        int idx = pick_top(mx, my);
        if (idx != -1) {
            bring_to_front(idx);
            ui_window_t* w = &g_wins[idx].win;
            wm_hittest_t hit = ui_chrome_hittest(w, mx, my);

            if (hit == HT_BTN_CLOSE) { wm_close_window(idx); return; }
            if (hit == HT_BTN_MAX) { wm_toggle_maximize(idx); return; }
            
            if (hit == HT_BTN_MIN) {
                app_t* app = g_wins[idx].owner;
                if (app) {
                    app->visible = 0;
                    g_active = -1;
                }
                return;
            }

            if (hit == HT_TITLE) {
                g_mouse_down = 1;
                g_drag_idx = idx;
                g_down_x = mx;
                g_down_y = my;
                g_dragging = 0;
                return;
            }
            
            app_t* app = g_wins[idx].owner;
            if (app && app->v && app->v->on_mouse) {
                app->v->on_mouse(app, mx, my, buttons, 0, 0);
            }
        }
    }

    if (released & 0x01) {
        g_mouse_down = 0; g_dragging = 0; g_drag_idx = -1;
    }
}

void wm_handle_mouse_move(int mx, int my) {
    int dx = mx - g_mouse_x;
    int dy = my - g_mouse_y;
    g_mouse_x = mx;
    g_mouse_y = my;

    if (g_mouse_down && g_drag_idx != -1) {
        ui_window_t* w = &g_wins[g_drag_idx].win;
        if (w->state != 1) { // Maximize değilse taşı
            w->x += dx;
            w->y += dy;
            g_dragging = 1;
        }
        return;
    }

    int top = pick_top(mx, my);
    if (top != -1) {
        app_t* app = g_wins[top].owner;
        if (app && app->v && app->v->on_mouse) {
            app->v->on_mouse(app, mx, my, 0, 0, 0);
        }
    }
}

void wm_draw(void) {
    for (int zi = 0; zi < g_count; ++zi) {
        int id = g_z[zi];
        ui_window_t* win = &g_wins[id].win;
        app_t* app = g_wins[id].owner;

        if (app && app->visible) {
            ui_window_draw(win, (id == g_active), g_mouse_x, g_mouse_y);
            if (app->v && app->v->on_draw) app->v->on_draw(app); 
        }
    }
}

// --- DESKTOP VE APP_MANAGER İÇİN GEREKLİ EKSİK FONKSİYONLAR ---

void wm_set_active(int win_id) {
    bring_to_front(win_id);
}

int wm_get_active_id(void) {
    return g_active;
}

int wm_is_any_window_captured(void) {
    return (g_mouse_down && g_drag_idx != -1);
}

ui_rect_t wm_get_client_rect(int win_id) {
    ui_window_t* w = &g_wins[win_id].win;
    ui_rect_t r = { .x = w->x + 2, .y = w->y + 24, .w = w->w - 4, .h = w->h - 26 };
    return r;
}

void wm_toggle_maximize(int idx) {
    ui_window_t* w = &g_wins[idx].win;
    if (w->state == 1) {
        w->x = w->prev_x; w->y = w->prev_y; w->w = w->prev_w; w->h = w->prev_h; w->state = 0;
    } else {
        w->prev_x = w->x; w->prev_y = w->y; w->prev_w = w->w; w->h = w->prev_h;
        w->x = 0; w->y = 24; w->w = fb_get_width(); w->h = fb_get_height() - 24; w->state = 1;
    }
}

int wm_find_window_at(int x, int y) { return pick_top(x, y); }
int wm_get_mouse_x(void) { return g_mouse_x; }
int wm_get_mouse_y(void) { return g_mouse_y; }