#include <stdint.h>
#include <ui/wm.h>
#include <ui/window/window.h>
#include <ui/wm/hittest.h>
#include <ui/window_chrome.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <app/app.h>
#include <ui/dialogs/save_dialog.h>
#include <ui/desktop.h>

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
static int g_mouse_consumed = 0;

static uint8_t g_buttons_state = 0;

extern uint32_t g_ticks_ms;

// --- YARDIMCI FONKSİYONLAR ---

static int is_window_interactive(const ui_window_t* w) {
    // Minimized pencere hit-test almaz, drag almaz
    return (w->state != WIN_MINIMIZED);
}

static int pick_top(int mx, int my) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
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
        if (g_wins[id].win.state != WIN_MINIMIZED) return id;
    }
    return -1;
}

static void bring_to_front(int win_id) {
    if (win_id < 0 || win_id >= g_count) return;

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

// --- CORE WM FONKSİYONLARI ---

void wm_init(void) {
    g_count = 0;
    g_active = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; ++i) g_z[i] = 0;
}

int wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner) {
    if (g_count >= WM_MAX_WINDOWS) return -1;

    int id = g_count;

    ui_window_t* win = &g_wins[id].win;
    win->x = x; win->y = y;
    win->w = w; win->h = h;

    // İlk değerleri koru
    win->prev_x = x;
    win->prev_y = y;
    win->prev_w = w;
    win->prev_h = h;

    win->title = title ? title : "Window";
    win->state = WIN_NORMAL;
    win->is_closed = 0;
    win->user_data = 0; // istersen owner'ı buraya da yazabiliriz
    // icon vs. kullanmıyorsan dokunma

    g_wins[id].owner = owner;

    g_z[id] = id;
    g_active = id;
    g_count++;

    return id;
}

void wm_close_window(int idx) {
    if (idx < 0 || idx >= g_count) return;

    // ✅ 1) Owner app’i kapatılmış olarak işaretle (singleton bug fix)
    app_t* app = g_wins[idx].owner;
    if (app) {
        // app destroy çağır (varsa)
        if (app->v && app->v->on_destroy) {
            app->v->on_destroy(app);
        }
        app->visible = 0;
        app->win_id = -1; // artık geçerli değil
    }

    // ✅ 2) array kaydır
    for (int i = idx; i < g_count - 1; i++) g_wins[i] = g_wins[i + 1];
    g_count--;

    // ✅ 3) z-order'dan idx'i çıkar
    for (int i = 0; i < g_count + 1; i++) {
        if (g_z[i] == idx) {
            for (int j = i; j < g_count; j++) g_z[j] = g_z[j + 1];
            break;
        }
    }

    // ✅ 4) idx üstündekileri -1 yap
    for (int i = 0; i < g_count; i++) {
        if (g_z[i] > idx) g_z[i]--;
    }

    // ✅ 5) active güncelle
    g_active = (g_count == 0) ? -1 : find_top_non_minimized();

    // ✅ 6) drag state temizle
    g_mouse_down = 0;
    g_dragging = 0;
    g_drag_idx = -1;
}

void wm_minimize(int idx) {
    if (idx < 0 || idx >= g_count) return;

    ui_window_t* w = &g_wins[idx].win;
    if (w->state == WIN_MINIMIZED) return;

    // Maximize ise önce normal’e al (isteğe bağlı, ama iyi his verir)
    if (w->state == WIN_MAXIMIZED) {
        // restore normal boyut
        w->x = w->prev_x; w->y = w->prev_y;
        w->w = w->prev_w; w->h = w->prev_h;
    }

    w->state = WIN_MINIMIZED;

    // Active ise başka pencereye geç
    if (g_active == idx) {
        int next = find_top_non_minimized();
        g_active = next; // -1 olabilir, okey
    }

    // Drag iptal
    g_mouse_down = 0;
    g_dragging = 0;
    g_drag_idx = -1;
}

void wm_restore(int idx) {
    if (idx < 0 || idx >= g_count) return;

    ui_window_t* w = &g_wins[idx].win;

    if (w->state == WIN_MINIMIZED) {
        w->state = WIN_NORMAL;
    }

    bring_to_front(idx);
}

void wm_toggle_minimize(int idx) {
    if (idx < 0 || idx >= g_count) return;

    if (g_wins[idx].win.state == WIN_MINIMIZED) wm_restore(idx);
    else wm_minimize(idx);
}

void wm_toggle_maximize(int idx) {
    if (idx < 0 || idx >= g_count) return;

    ui_window_t* w = &g_wins[idx].win;

    // Minimized ise önce restore et
    if (w->state == WIN_MINIMIZED) {
        w->state = WIN_NORMAL;
    }

    if (w->state == WIN_MAXIMIZED) {
        // restore
        w->x = w->prev_x; w->y = w->prev_y;
        w->w = w->prev_w; w->h = w->prev_h;
        w->state = WIN_NORMAL;
    } else {
        // save prev (BUGFIX: prev_h yanlış yazılmıştı)
        w->prev_x = w->x; w->prev_y = w->y;
        w->prev_w = w->w; w->prev_h = w->h;

        w->x = 0;
        w->y = 24;
        w->w = fb_get_width();
        w->h = fb_get_height() - 24;
        w->state = WIN_MAXIMIZED;

        bring_to_front(idx);
    }
}

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

    // Hangi pencere üstte?
    int idx = pick_top(mx, my);

    // Pencere yoksa: sadece drag reset vb.
    if (idx == -1) {
        if (released & 0x01) {
            g_mouse_down = 0;
            g_dragging = 0;
            g_drag_idx = -1;
        }
        return;
    }

    // Pencere var -> consume
    g_mouse_consumed = 1;

    // Pressed ise öne al + chrome hittest + drag başlat
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
        // HT_CLIENT ise aşağıda app'e event gidecek
    }

    // ✅ App mouse event: pressed/released olmasa bile GİTSİN
    // (hover + release için şart)
    {
        app_t* app = g_wins[idx].owner;
        if (app && app->v && app->v->on_mouse) {
            app->v->on_mouse(app, mx, my, buttons, pressed, released);
        }
    }

    // Release -> drag reset
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

    if (g_mouse_down && g_drag_idx != -1) {
        ui_window_t* w = &g_wins[g_drag_idx].win;

        // Maximize değilse taşı (minimized zaten drag'e giremez)
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
            app->v->on_mouse(app, mx, my, g_buttons_state, 0, 0);
        }
    }
}

void wm_draw(void) {
    for (int zi = 0; zi < g_count; ++zi) {
        int id = g_z[zi];
        ui_window_t* win = &g_wins[id].win;
        app_t* app = g_wins[id].owner;

        // Minimized çizilmez
        if (win->state == WIN_MINIMIZED) continue;

        // Senin sistemde app->visible kontrolü vardı.
        // İstersen burada tamamen WM state’e geçip visible'ı kaldırırız.
        if (app && app->visible) {

            ui_window_draw(win, (id == g_active), g_mouse_x, g_mouse_y);

            if (app->v && app->v->on_draw) {

                ui_rect_t client = wm_get_client_rect(id);

                // 🔥 origin set
                gfx_set_origin(client.x, client.y);

                // 🔥 burada clip sistemi yoksa şimdilik atlayabiliriz

                app->v->on_draw(app);

                // 🔥 origin reset
                gfx_reset_origin();
            }
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

bool wm_is_window_alive(int win_id) {
    return (win_id >= 0 && win_id < g_count);
}

ui_rect_t wm_get_client_rect(int win_id) {
    ui_rect_t r = {0,0,0,0};
    if (win_id < 0 || win_id >= g_count) return r;
    ui_window_t* w = &g_wins[win_id].win;
    r.x = w->x + 2; r.y = w->y + 24; r.w = w->w - 4; r.h = w->h - 26;
    return r;
}

int wm_find_window_at(int x, int y) { return pick_top(x, y); }

int wm_get_mouse_x(void) { return g_mouse_x; }
int wm_get_mouse_y(void) { return g_mouse_y; }

int wm_get_count(void) {
    return g_count;
}

const ui_window_t* wm_get_window_ptr(int idx) {
    if (idx < 0 || idx >= g_count) return 0;
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