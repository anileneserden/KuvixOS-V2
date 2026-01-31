// src/lib/ui/wm/wm.c
#include <stdint.h>
#include <ui/wm.h>
#include <ui/window.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <app/app.h>

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
static int g_z[WM_MAX_WINDOWS];  // draw order: win_id list
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

static const int WM_BORDER  = 2;
static const int WM_TITLE_H = 24;
static const ui_padding_t WM_PAD = { .left=10, .top=10, .right=10, .bottom=10 };

// dosyanın başına ekle (dc click position için)
static int dc_x = 0;
static int dc_y = 0;

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
    return (mx >= btn_x && mx < btn_x + btn_size &&
            my >= btn_y && my < btn_y + btn_size);
}

static int in_max_btn(const ui_window_t* w, int mx, int my) {
    const int btn_size = 16;
    const int btn_y = w->y + 4;
    const int close_x = w->x + w->w - btn_size - 4;
    const int x = close_x - btn_size - 4;
    return (mx >= x && mx < x + btn_size &&
            my >= btn_y && my < btn_y + btn_size);
}

static int in_min_btn(const ui_window_t* w, int mx, int my) {
    const int btn_size = 16;
    const int btn_y = w->y + 4;
    const int close_x = w->x + w->w - btn_size - 4;
    const int max_x   = close_x - btn_size - 4;
    const int x       = max_x   - btn_size - 4;
    return (mx >= x && mx < x + btn_size &&
            my >= btn_y && my < btn_y + btn_size);
}

static int in_resize_grip(const ui_window_t* w, int px, int py) {
    int gx = w->x + w->w - RESIZE_GRIP;
    int gy = w->y + w->h - RESIZE_GRIP;
    return (px >= gx && px < w->x + w->w && py >= gy && py < w->y + w->h);
}

static int pick_top(int mx, int my) {
    for (int zi = g_count - 1; zi >= 0; --zi) {
        int id = g_z[zi];
        if (contains(&g_wins[id].win, mx, my)) return id; // WIN_ID döndür
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

    g_active = win_id; // aktif ID artık gerçek win_id
}

void wm_init(void) {
    g_count = 0;
    g_active = -1;
    g_dragging = 0;
    g_drag_idx = -1;
    
    for (int i = 0; i < WM_MAX_WINDOWS; ++i) g_z[i] = 0;
}

int wm_add_window(int x, int y, int w, int h, const char* title, app_t* owner)
{
    if (g_count >= WM_MAX_WINDOWS) return -1;

    g_wins[g_count].win = (ui_window_t){
        .x = x, .y = y, .w = w, .h = h, .title = title
    };
    g_wins[g_count].owner = owner;

    g_z[g_count] = g_count;
    g_active = g_count;

    g_count++;
    return g_count - 1;
}

void wm_close_window(int idx)
{
    if (idx < 0 || idx >= g_count) return;

    // 1) varsa owner cleanup (opsiyonel)
    // app_t* owner = g_wins[idx].owner;
    // if (owner && owner->v && owner->v->on_close) owner->v->on_close(owner);

    // 2) g_wins'i kaydır
    for (int i = idx; i < g_count - 1; i++) {
        g_wins[i] = g_wins[i + 1];
    }
    g_count--;

    // 3) z-order: idx üstündeki id'leri 1 azalt + idx'i listeden çıkar
    for (int i = 0; i < g_count + 1; i++) {
        if (g_z[i] == idx) {
            // burayı sil: kaydır
            for (int j = i; j < g_count; j++) g_z[j] = g_z[j + 1];
            break;
        }
    }
    for (int i = 0; i < g_count; i++) {
        if (g_z[i] > idx) g_z[i]--;
    }

    // 4) active düzelt
    if (g_count == 0) g_active = -1;
    else {
        // en üstteki aktif olsun
        g_active = g_z[g_count - 1];
    }

    // drag state temizle
    g_mouse_down = 0;
    g_dragging = 0;
    g_drag_idx = -1;
    g_drag_restore_pending = 0;
}

int wm_get_window(int win_id, ui_window_t* out)
{
    if (!out) return 0;
    if (win_id < 0 || win_id >= g_count) return 0;
    *out = g_wins[win_id].win;
    return 1;
}

static ui_rect_t g_draw_client_rect;

const ui_rect_t* wm_get_draw_client_rect(void)
{
    return &g_draw_client_rect;
}

void wm_set_title(int win_id, const char* title)
{
    if (win_id < 0 || win_id >= g_count) return;
    g_wins[win_id].win.title = title;
}

int wm_get_active_id(void) { return g_active; }

app_t* wm_get_active_owner(void)
{
    if (g_active < 0 || g_active >= g_count) return 0;
    return g_wins[g_active].owner;
}

void wm_handle_mouse(int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons)
{
    g_mouse_x = mx;
    g_mouse_y = my;

    uint32_t now = g_ticks_ms;

    // double-click pending timeout
    if (dc_pending && (now - dc_time) > DOUBLE_CLICK_MS) {
        dc_pending = 0;
        dc_time = 0;
        dc_win = -1;
    }

    /* ---------------- PRESS ---------------- */
    if (pressed & 0x01) {
        int idx = pick_top(mx, my);
        if (idx != -1) {
            bring_to_front(idx);
            idx = g_active;

            ui_window_t* w = &g_wins[idx].win;

            // butonlarda double-click olmasın
            if (in_close_btn(w, mx, my)) { wm_close_window(idx); dc_pending=0; dc_win=-1; dc_time=0; return; }
            if (in_max_btn(w, mx, my))   { wm_toggle_maximize(idx); dc_pending=0; dc_win=-1; dc_time=0; return; }
            if (in_min_btn(w, mx, my))   { wm_minimize(idx); dc_pending=0; dc_win=-1; dc_time=0; return; }

            // --- RESIZE GRIP: sağ-alt köşe ---
            if (w->state != WIN_MAXIMIZED && in_resize_grip(w, mx, my)) {
                g_resizing = 1;
                g_resize_idx = idx;
                g_resize_start_mx = mx;
                g_resize_start_my = my;
                g_resize_start_w  = w->w;
                g_resize_start_h  = w->h;

                // drag state kapat
                g_mouse_down = 0;
                g_dragging = 0;
                g_drag_idx = -1;
                g_drag_restore_pending = 0;

                // double click iptal
                dc_pending = 0;
                dc_win = -1;
                dc_time = 0;

                return; // WM olayı tüketti
            }

            // titlebar boş alanı
            if (in_titlebar(w, mx, my)) {

                int dx = mx - dc_x;
                int dy = my - dc_y;
                int same_spot = (dx*dx + dy*dy) <= (DOUBLE_CLICK_DIST * DOUBLE_CLICK_DIST);

                if (dc_pending && dc_win == idx && same_spot && (now - dc_time) <= DOUBLE_CLICK_MS) {
                    // 2. tık: double
                    wm_toggle_maximize(idx);
                    dc_pending = 0;
                    dc_time = 0;
                    dc_win = -1;
                } else {
                    // 1. tık: pending başlat
                    dc_pending = 1;
                    dc_time = now;
                    dc_win = idx;
                    dc_x = mx;
                    dc_y = my;
                }

                // drag hazırlığı
                g_mouse_down = 1;
                g_drag_idx = idx;
                g_down_x = mx;
                g_down_y = my;
                g_dragging = 0;

                if (w->state == WIN_MAXIMIZED) g_drag_restore_pending = 1;
                else {
                    g_drag_restore_pending = 0;
                    g_drag_off_x = mx - w->x;
                    g_drag_off_y = my - w->y;
                }

                // titlebar press WM tarafından handle edildi
                // ama app'e de yine event göndereceğiz (altta)
            } else {
                // titlebar değilse: WM drag başlatmaz
                g_mouse_down = 0;
                g_dragging = 0;
                g_drag_idx = -1;
                g_drag_restore_pending = 0;
            }
        } else {
            // hiçbir pencereye basılmadı
            g_mouse_down = 0;
            g_dragging = 0;
            g_drag_idx = -1;
            g_drag_restore_pending = 0;
        }
    }

    /* ---------------- MOVE ---------------- */

    // --- RESIZE MOVE ---
    if (g_resizing && (buttons & 0x01) && g_resize_idx >= 0 && g_resize_idx < g_count) {
        ui_window_t* w = &g_wins[g_resize_idx].win;

        int dx = mx - g_resize_start_mx;
        int dy = my - g_resize_start_my;

        int nw = g_resize_start_w + dx;
        int nh = g_resize_start_h + dy;

        if (nw < WIN_MIN_W) nw = WIN_MIN_W;
        if (nh < WIN_MIN_H) nh = WIN_MIN_H;

        // ekran sınırı clamp
        int max_w = fb_get_width()  - w->x;
        int max_h = fb_get_height() - w->y;
        if (nw > max_w) nw = max_w;
        if (nh > max_h) nh = max_h;

        w->w = nw;
        w->h = nh;

        // resize sırasında double click iptal
        dc_pending = 0;
        dc_win = -1;
        dc_time = 0;

        return; // resize WM tarafından tüketildi
    }

    // --- DRAG MOVE (titlebar drag) ---
    if (g_mouse_down && (buttons & 0x01)) {
        int dx = mx - g_down_x;
        int dy = my - g_down_y;

        if (!g_dragging) {
            if (dx*dx + dy*dy >= DRAG_THRESHOLD*DRAG_THRESHOLD) {
                g_dragging = 1;

                // drag başladı → artık double click olamaz
                dc_pending = 0;
                dc_win = -1;
                dc_time = 0;

                ui_window_t* w = &g_wins[g_drag_idx].win;

                // maximize iken sürükleme ile restore
                if (g_drag_restore_pending && w->state == WIN_MAXIMIZED) {
                    w->state = WIN_NORMAL;
                    w->x = w->prev_x;
                    w->y = w->prev_y;
                    w->w = w->prev_w;
                    w->h = w->prev_h;

                    g_drag_off_x = w->w / 2;
                    g_drag_off_y = 12;

                    g_drag_restore_pending = 0;
                }
            }
        }

        if (g_dragging) {
            ui_window_t* w = &g_wins[g_drag_idx].win;
            w->x = mx - g_drag_off_x;
            w->y = my - g_drag_off_y;
        }
    }

    /* ---------------- RELEASE ---------------- */
    if (released & 0x01) {

        if (g_resizing) {
            g_resizing = 0;
            g_resize_idx = -1;
        }

        g_mouse_down = 0;
        g_dragging = 0;
        g_drag_idx = -1;
        g_drag_restore_pending = 0;
    }

    /* app mouse event */
    app_t* owner = wm_get_active_owner();
    if (owner && owner->v && owner->v->on_mouse) {
        owner->v->on_mouse(owner, mx, my, pressed, released, buttons);
    }
}


ui_rect_t wm_get_client_rect(int win_id)
{
    ui_window_t w;
    ui_rect_t r = {0,0,0,0};
    if (!wm_get_window(win_id, &w)) return r;

    int x0 = w.x + WM_BORDER + WM_PAD.left;
    int y0 = w.y + WM_TITLE_H + WM_BORDER + WM_PAD.top;

    int x1 = w.x + w.w - WM_BORDER - WM_PAD.right;
    int y1 = w.y + w.h - WM_BORDER - WM_PAD.bottom;

    r.x = x0;
    r.y = y0;
    r.w = (x1 > x0) ? (x1 - x0) : 0;
    r.h = (y1 > y0) ? (y1 - y0) : 0;
    return r;
}


void wm_toggle_maximize(int idx)
{
    ui_window_t* w = &g_wins[idx].win;

    if (w->state == WIN_MAXIMIZED) {
        w->x = w->prev_x;
        w->y = w->prev_y;
        w->w = w->prev_w;
        w->h = w->prev_h;
        w->state = WIN_NORMAL;
    } else {
        w->prev_x = w->x;
        w->prev_y = w->y;
        w->prev_w = w->w;
        w->prev_h = w->h;

        w->x = 0;
        w->y = 24;
        w->w = fb_get_width();
        w->h = fb_get_height() - 24;
        w->state = WIN_MAXIMIZED;
    }
}

void wm_minimize(int idx) { (void)idx; }

void wm_draw(void)
{
    for (int zi = 0; zi < g_count; ++zi) {
        int id = g_z[zi];
        int active = (id == g_active);

        ui_window_draw(&g_wins[id].win, active, g_mouse_x, g_mouse_y);

        app_t* owner = g_wins[id].owner;
        if (owner && owner->v && owner->v->on_draw) {
            g_draw_client_rect = wm_get_client_rect(id);
            owner->v->on_draw(owner);
        }
    }
}

int wm_dbg_dc_pending(void) { return dc_pending; }

uint32_t wm_dbg_dc_age_ms(uint32_t now_ms)
{
    if (!dc_pending) return 0;
    return now_ms - dc_time;
}

int wm_dbg_dc_win(void) { return dc_win; }
