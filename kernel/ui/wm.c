// kernel/ui/wm.c

#include <stdint.h>
#include <stdbool.h>

#include <ui/wm.h>
#include <ui/window/window.h>
#include <ui/wm/hittest.h>
#include <ui/window_chrome.h>
#include <ui/theme.h>              // ✅ eklendi (ui_get_theme)

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>

#include <app/app.h>
#include <app/app_manager.h>

#include <ui/dialogs/save_dialog.h>
#include <ui/desktop.h>

#include <kernel/printk.h>

#define WM_MAX_WINDOWS 20

typedef struct {
    ui_window_t win;
    app_t* owner;
} wm_entry_t;

static wm_entry_t g_wins[WM_MAX_WINDOWS];

// ✅ slot dolu mu?
static uint8_t g_used[WM_MAX_WINDOWS];

// ✅ z-order listesi (sadece alive id’ler)
// g_count = z listesi uzunluğu
static int g_z[WM_MAX_WINDOWS];
static int g_count = 0;

static int g_active = -1;

// mouse/drag state
static int g_mouse_down = 0;
static int g_dragging = 0;
static int g_drag_idx = -1;

static int g_down_x = 0;
static int g_down_y = 0;

static int g_mouse_x = 0;
static int g_mouse_y = 0;

static int g_mouse_consumed = 0;
static uint8_t g_buttons_state = 0;

extern uint32_t g_ticks_ms;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

static int is_alive_id(int id) {
    return (id >= 0 && id < WM_MAX_WINDOWS && g_used[id]);
}

static int is_window_interactive(const ui_window_t* w) {
    // Minimized pencere hit-test almaz, drag almaz
    return (w->state != WIN_MINIMIZED);
}

static int pick_top(int mx, int my) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
        if (!is_alive_id(id)) continue;

        ui_window_t* w = &g_wins[id].win;
        if (!is_window_interactive(w)) continue;

        if (mx >= w->x && mx < (w->x + w->w) &&
            my >= w->y && my < (w->y + w->h)) {
            return id;
        }
    }
    return -1;
}

static int find_top_non_minimized(void) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
        if (!is_alive_id(id)) continue;
        if (g_wins[id].win.state != WIN_MINIMIZED) return id;
    }
    return -1;
}

static void bring_to_front(int win_id) {
    if (!is_alive_id(win_id)) return;

    // Minimized pencereyi öne alma (önce restore edilmeli)
    if (g_wins[win_id].win.state == WIN_MINIMIZED) return;

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

static int z_remove(int win_id) {
    for (int i = 0; i < g_count; i++) {
        if (g_z[i] == win_id) {
            for (int j = i; j < g_count - 1; j++) g_z[j] = g_z[j + 1];
            g_count--;
            return 1;
        }
    }
    return 0;
}

static int z_contains(int win_id) {
    for (int i = 0; i < g_count; i++) {
        if (g_z[i] == win_id) return 1;
    }
    return 0;
}

// ------------------------------------------------------------
// WM Core
// ------------------------------------------------------------

void wm_init(void) {
    g_count = 0;
    g_active = -1;

    g_mouse_down = 0;
    g_dragging = 0;
    g_drag_idx = -1;

    g_mouse_x = 0;
    g_mouse_y = 0;

    g_mouse_consumed = 0;
    g_buttons_state = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; ++i) {
        g_z[i] = 0;
        g_used[i] = 0;
        g_wins[i].owner = 0;

        // güvenli başlangıç
        g_wins[i].win.is_closed = 1;
        g_wins[i].win.state = WIN_NORMAL;
        g_wins[i].win.x = g_wins[i].win.y = 0;
        g_wins[i].win.w = g_wins[i].win.h = 0;
        g_wins[i].win.title = "Window";
        g_wins[i].win.user_data = 0;
    }
}

int wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner) {
    if (g_count >= WM_MAX_WINDOWS) return -1; // z listesi dolu

    // ✅ free slot bul
    int id = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_used[i]) { id = i; break; }
    }
    if (id < 0) return -1;

    ui_window_t* win = &g_wins[id].win;
    win->x = x; win->y = y;
    win->w = w; win->h = h;

    // prev değerleri
    win->prev_x = x;
    win->prev_y = y;
    win->prev_w = w;
    win->prev_h = h;

    win->title = title ? title : "Window";
    win->state = WIN_NORMAL;
    win->is_closed = 0;
    win->user_data = 0;

    g_wins[id].owner = owner;

    // ✅ z-order listesine ekle
    g_z[g_count++] = id;
    g_active = id;
    g_used[id] = 1;

    return id;
}

static ui_rect_t rect_union(ui_rect_t a, ui_rect_t b) {
    int x1 = (a.x < b.x) ? a.x : b.x;
    int y1 = (a.y < b.y) ? a.y : b.y;
    int x2 = ((a.x + a.w) > (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    int y2 = ((a.y + a.h) > (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    ui_rect_t r = { x1, y1, x2 - x1, y2 - y1 };
    return r;
}

void wm_set_window_size(int win_id, int w, int h) {
    if (!is_alive_id(win_id)) return;

    if (w < 80) w = 80;
    if (h < 60) h = 60;

    ui_window_t* win = &g_wins[win_id].win;
    if (win->state != WIN_NORMAL) return;

    // OLD outer
    ui_rect_t oldr = { win->x, win->y, win->w, win->h };

    // apply new size
    win->w = w;
    win->h = h;
    win->prev_w = w;
    win->prev_h = h;

    // NEW outer
    ui_rect_t newr = { win->x, win->y, win->w, win->h };

    ui_rect_t dirty = rect_union(oldr, newr);

    // Eğer varsa:
    // desktop_invalidate_rect(dirty.x, dirty.y, dirty.w, dirty.h);

    printk("[WM] set_window_size id=%d -> %dx%d\n", win_id, w, h);

    // Yoksa şimdilik en azından full:
    desktop_invalidate_full();

    printk("[WM] resize win_id=%d old=%dx%d new=%dx%d dirty=%d,%d %dx%d\n",
           win_id, oldr.w, oldr.h, newr.w, newr.h, dirty.x, dirty.y, dirty.w, dirty.h);
}

void wm_close_window(int win_id) {
    if (!is_alive_id(win_id)) return;

    // ✅ Drag state temizle (kapatılan pencere sürükleniyorsa)
    if (g_drag_idx == win_id) {
        g_mouse_down = 0;
        g_dragging = 0;
        g_drag_idx = -1;
    }

    // ✅ App cleanup TEK yerde: AppManager
    appmgr_on_window_closed(win_id);

    // ✅ WM slot boşalt
    g_wins[win_id].owner = 0;
    g_wins[win_id].win.is_closed = 1;
    g_used[win_id] = 0;

    // ✅ z-order’dan çıkar
    z_remove(win_id);

    // ✅ active güncelle
    if (g_active == win_id) {
        g_active = (g_count == 0) ? -1 : find_top_non_minimized();
    }
}

void wm_minimize(int win_id) {
    if (!is_alive_id(win_id)) return;

    ui_window_t* w = &g_wins[win_id].win;
    if (w->state == WIN_MINIMIZED) return;

    // Maximize ise önce normal’e dön
    if (w->state == WIN_MAXIMIZED) {
        w->x = w->prev_x; w->y = w->prev_y;
        w->w = w->prev_w; w->h = w->prev_h;
    }

    w->state = WIN_MINIMIZED;

    // Active ise başka pencereye geç
    if (g_active == win_id) {
        g_active = find_top_non_minimized();
    }

    // Drag iptal
    g_mouse_down = 0;
    g_dragging = 0;
    g_drag_idx = -1;
}

void wm_restore(int win_id) {
    if (!is_alive_id(win_id)) return;

    ui_window_t* w = &g_wins[win_id].win;
    if (w->state == WIN_MINIMIZED) {
        w->state = WIN_NORMAL;
    }

    bring_to_front(win_id);
}

void wm_toggle_minimize(int win_id) {
    if (!is_alive_id(win_id)) return;

    if (g_wins[win_id].win.state == WIN_MINIMIZED) wm_restore(win_id);
    else wm_minimize(win_id);
}

void wm_toggle_maximize(int win_id) {
    if (!is_alive_id(win_id)) return;

    ui_window_t* w = &g_wins[win_id].win;

    // Minimized ise restore et
    if (w->state == WIN_MINIMIZED) {
        w->state = WIN_NORMAL;
    }

    if (w->state == WIN_MAXIMIZED) {
        // restore
        w->x = w->prev_x; w->y = w->prev_y;
        w->w = w->prev_w; w->h = w->prev_h;
        w->state = WIN_NORMAL;
    } else {
        // save prev
        w->prev_x = w->x; w->prev_y = w->y;
        w->prev_w = w->w; w->prev_h = w->h;

        // NOTE: topbar 24 sabit sende; istersen bunu da theme’den alırsın
        w->x = 0;
        w->y = 24;
        w->w = (int)fb_get_width();
        w->h = (int)fb_get_height() - 24;
        w->state = WIN_MAXIMIZED;

        bring_to_front(win_id);
    }
}

// ------------------------------------------------------------
// Mouse handling
// ------------------------------------------------------------

void wm_handle_mouse(int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    g_mouse_x = mx;
    g_mouse_y = my;
    g_buttons_state = buttons;
    g_mouse_consumed = 0;

    // 1) Modal save dialog
    if (save_dialog_is_active()) {
        save_dialog_handle_mouse(mx, my, (pressed & 0x01));
        if (pressed & 0x01) g_mouse_consumed = 1;
        return;
    }

    int idx = pick_top(mx, my);

    // pencere yoksa
    if (idx == -1) {
        if (released & 0x01) {
            g_mouse_down = 0;
            g_dragging = 0;
            g_drag_idx = -1;
        }
        return;
    }

    // pencere var -> consume
    g_mouse_consumed = 1;

    // pressed: öne al + chrome hittest
    if (pressed & 0x01) {
        bring_to_front(idx);

        ui_window_t* w = &g_wins[idx].win;
        wm_hittest_t hit = ui_chrome_hittest(w, mx, my);

        if (hit == HT_BTN_CLOSE) {
            app_t* app = g_wins[idx].owner;
            if (app && app->v && app->v->on_close_request) {
                int allow = app->v->on_close_request(app);
                if (!allow) return;
            }
            wm_close_window(idx);
            return;
        }

        if (hit == HT_BTN_MAX) { wm_toggle_maximize(idx); return; }
        if (hit == HT_BTN_MIN) { wm_minimize(idx); return; }

        if (hit == HT_TITLE) {
            g_mouse_down = 1;
            g_drag_idx = idx;
            g_down_x = mx;
            g_down_y = my;
            g_dragging = 0;
            return;
        }
        // HT_CLIENT ise app event aşağıda gidecek
    }

    // ✅ app mouse event her zaman gitsin (hover/release için)
    {
        app_t* app = g_wins[idx].owner;
        if (app && app->v && app->v->on_mouse) {
            ui_rect_t cr = wm_get_client_rect(idx);

            // mx,my ekran coords -> client coords
            int cx = mx - cr.x;
            int cy = my - cr.y;

            // param sıra: pressed, released, buttons
            app->v->on_mouse(app, cx, cy, pressed, released, buttons);
        }
    }

    // release -> drag reset
    if (released & 0x01) {
        g_mouse_down = 0;
        g_dragging = 0;
        g_drag_idx = -1;
    }
}

void wm_handle_mouse_move(int mx, int my) {
    int dx = mx - g_mouse_x;
    int dy = my - g_mouse_y;

    g_mouse_x = mx;
    g_mouse_y = my;

    if (g_mouse_down && g_drag_idx != -1 && is_alive_id(g_drag_idx)) {
        ui_window_t* w = &g_wins[g_drag_idx].win;

        // maximize değilse taşı
        if (w->state != WIN_MAXIMIZED) {
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
            ui_rect_t cr = wm_get_client_rect(top);

            int cx = mx - cr.x;
            int cy = my - cr.y;

            app->v->on_mouse(app, cx, cy, 0, 0, g_buttons_state);
        }
    }
}

void wm_handle_mouse_wheel(int mx, int my, int wheel, uint8_t buttons) {
    g_buttons_state = buttons;
    g_mouse_x = mx;
    g_mouse_y = my;

    // 1) modal varsa önce ona ver
    if (save_dialog_is_active()) {
        return;
    }

    // 2) mouse altındaki pencere
    int idx = pick_top(mx, my);

    // 3) eğer altında pencere yoksa active’e gönder
    if (idx == -1) idx = g_active;

    if (!is_alive_id(idx)) return;

    // minimized’a wheel verme
    if (g_wins[idx].win.state == WIN_MINIMIZED) return;

    app_t* app = g_wins[idx].owner;
    if (app && app->v && app->v->on_wheel) {
        desktop_invalidate_full();
        app->v->on_wheel(app, wheel);
    }
}

// ------------------------------------------------------------
// Draw
// ------------------------------------------------------------

void wm_draw(void) {
    for (int zi = 0; zi < g_count; ++zi) {
        int id = g_z[zi];
        if (!is_alive_id(id)) continue;

        ui_window_t* win = &g_wins[id].win;
        app_t* app = g_wins[id].owner;

        // minimized çizme
        if (win->state == WIN_MINIMIZED) continue;

        if (app && app->visible) {
            ui_window_draw(win, (id == g_active), g_mouse_x, g_mouse_y);

            if (app->v && app->v->on_draw) {
                ui_rect_t client = wm_get_client_rect(id);

                gfx_set_origin(client.x, client.y);
                app->v->on_draw(app);
                gfx_reset_origin();
            }
        }
    }
}

// ------------------------------------------------------------
// Public helpers
// ------------------------------------------------------------

void wm_set_active(int win_id) {
    bring_to_front(win_id);
}

int wm_get_active_id(void) {
    return g_active;
}

void wm_set_active_id(int win_id) {
    g_active = win_id;
}

int wm_is_any_window_captured(void) {
    return (g_mouse_down && g_drag_idx != -1);
}

bool wm_is_window_alive(int win_id) {
    return is_alive_id(win_id);
}

ui_rect_t wm_get_client_rect(int win_id) {
    ui_rect_t r = {0,0,0,0};
    if (!is_alive_id(win_id)) return r;

    ui_window_t* w = &g_wins[win_id].win;

    const ui_theme_t* th = ui_get_theme();
    int border  = 2;
    int title_h = 24;

    if (th) {
        border  = th->window_border_px;
        title_h = th->window_title_h;
    }
    if (border < 1) border = 1;
    if (title_h < 18) title_h = 18;

    r.x = w->x + border;
    r.y = w->y + title_h;
    r.w = w->w - (border * 2);
    r.h = w->h - title_h - border;

    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

int wm_find_window_at(int x, int y) {
    return pick_top(x, y);
}

int wm_get_mouse_x(void) { return g_mouse_x; }
int wm_get_mouse_y(void) { return g_mouse_y; }

int wm_get_count(void) {
    return g_count; // z-list count (alive windows)
}

const ui_window_t* wm_get_window_ptr(int idx) {
    // burada idx "win_id" olarak kullanılıyor
    if (!is_alive_id(idx)) return 0;
    return &g_wins[idx].win;
}

int wm_get_z(int z_index) {
    if (z_index < 0 || z_index >= g_count) return -1;
    return g_z[z_index];
}

int wm_did_consume_mouse(void) {
    return g_mouse_consumed;
}

int wm_is_dragging_window(void) {
    return g_dragging;
}