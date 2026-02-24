#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <stdint.h>
#include <stdbool.h>
#include <ui/apps/notepad_demo.h>

// --- DIŞ BİLDİRİMLER (Notepad ile aynı) ---
extern int  wm_get_mouse_x(void);
extern int  wm_get_mouse_y(void);
extern uint32_t g_ticks_ms;

// Menü itemları (demo)
static const char* demo_menu_items[] = { "Yeni", "Aç", "Kaydet", "Kapat" };

// ------------------------------------------------------------
// Mouse ekran coords -> client-relative (Notepad'dekiyle aynı)
// ------------------------------------------------------------
static inline void mouse_to_client(const ui_rect_t* client, int mx, int my, int* out_lx, int* out_ly)
{
    if (mx >= client->x && mx < client->x + client->w &&
        my >= client->y && my < client->y + client->h) {
        *out_lx = mx - client->x;
        *out_ly = my - client->y;
    } else {
        *out_lx = mx;
        *out_ly = my;
    }
}

static inline bool pt_in(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

// ------------------------------------------------------------
// APP CALLBACK'LERİ
// ------------------------------------------------------------
static void notepad_demo_on_create(app_t* self) {
    notepad_demo_t* d = (notepad_demo_t*)self->user;
    if (!d) return;

    d->window_id = self->win_id;
    d->active = true;

    d->menu_open = false;
    d->editor_focus = false;
    d->status_visible = true;

    d->caret_last_ms  = g_ticks_ms;
    d->caret_blink_ms = 0;
    d->caret_visible  = 1;
}

static void notepad_demo_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_demo_t* d = (notepad_demo_t*)self->user;

    // ------------------------------------------------------------
    // Caret blink (Notepad ile aynı mantık)
    // ------------------------------------------------------------
    uint32_t now = g_ticks_ms;
    uint32_t dt  = now - d->caret_last_ms;
    d->caret_last_ms = now;

    if (dt > 2000) dt = 0;

    d->caret_blink_ms += dt;
    if (d->caret_blink_ms >= 500) {
        d->caret_blink_ms %= 500;
        d->caret_visible = !d->caret_visible;
        wm_invalidate();
    }

    ui_rect_t client = wm_get_client_rect(self->win_id);

    // mouse ekran coords -> client-relative
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();
    int lx, ly;
    mouse_to_client(&client, mx, my, &lx, &ly);

    // ------------------------------------------------------------
    // UI ölçüleri (KWrite/KNote lite)
    // ------------------------------------------------------------
    const int MENUBAR_H = 22;
    const int TOOLBAR_H = 26;
    const int STATUS_H  = 20;

    const int FILE_BTN_X = 8;
    const int FILE_BTN_Y = 3;
    const int FILE_BTN_W = 55;
    const int FILE_BTN_H = 16;

    // Toolbar butonları (basit)
    const int TB_Y = MENUBAR_H + 4;
    const int TB_BTN_H = 18;

    int tb_x1 = 8;
    int tb_w1 = 42; // Yeni
    int tb_x2 = tb_x1 + tb_w1 + 6;
    int tb_w2 = 42; // Aç
    int tb_x3 = tb_x2 + tb_w2 + 6;
    int tb_w3 = 60; // Kaydet

    // Editor alanı
    int top = MENUBAR_H + TOOLBAR_H;
    int bottom = d->status_visible ? STATUS_H : 0;
    int view_h = client.h - top - bottom;

    int pad = 10;
    int ex = pad;
    int ey = top + pad;
    int ew = client.w - pad * 2;
    int eh = view_h - pad * 2;

    // ------------------------------------------------------------
    // Background
    // ------------------------------------------------------------
    gfx_fill_rect(0, 0, client.w, client.h, 0xEAEAEA);

    // ------------------------------------------------------------
    // Menu bar
    // ------------------------------------------------------------
    gfx_fill_rect(0, 0, client.w, MENUBAR_H, 0xDEDEDE);
    gfx_draw_rect(0, 0, client.w, MENUBAR_H, 0xA0A0A0);

    bool file_hover = pt_in(lx, ly, FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H);
    if (d->menu_open) {
        gfx_fill_rect(FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H, 0xC0C0C0);
    } else if (file_hover) {
        gfx_draw_rect(FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H, 0xFFFFFF);
    }
    gfx_draw_text_utf8(FILE_BTN_X + 8, FILE_BTN_Y + 2, 0x000000, "Dosya");

    // başlık
    gfx_draw_text_utf8(120, 5, 0x444444, "Kuvix Note (Demo)");

    // ------------------------------------------------------------
    // Toolbar
    // ------------------------------------------------------------
    gfx_fill_rect(0, MENUBAR_H, client.w, TOOLBAR_H, 0xD8D8D8);
    gfx_draw_rect(0, MENUBAR_H, client.w, TOOLBAR_H, 0xA0A0A0);

    // Yeni
    bool h1 = pt_in(lx, ly, tb_x1, TB_Y, tb_w1, TB_BTN_H);
    if (h1) gfx_fill_rect(tb_x1, TB_Y, tb_w1, TB_BTN_H, 0xCFCFCF);
    gfx_draw_rect(tb_x1, TB_Y, tb_w1, TB_BTN_H, 0xA0A0A0);
    gfx_draw_text_utf8(tb_x1 + 10, TB_Y + 2, 0x000000, "Yeni");

    // Aç
    bool h2 = pt_in(lx, ly, tb_x2, TB_Y, tb_w2, TB_BTN_H);
    if (h2) gfx_fill_rect(tb_x2, TB_Y, tb_w2, TB_BTN_H, 0xCFCFCF);
    gfx_draw_rect(tb_x2, TB_Y, tb_w2, TB_BTN_H, 0xA0A0A0);
    gfx_draw_text_utf8(tb_x2 + 12, TB_Y + 2, 0x000000, "Aç");

    // Kaydet
    bool h3 = pt_in(lx, ly, tb_x3, TB_Y, tb_w3, TB_BTN_H);
    if (h3) gfx_fill_rect(tb_x3, TB_Y, tb_w3, TB_BTN_H, 0xCFCFCF);
    gfx_draw_rect(tb_x3, TB_Y, tb_w3, TB_BTN_H, 0xA0A0A0);
    gfx_draw_text_utf8(tb_x3 + 10, TB_Y + 2, 0x000000, "Kaydet");

    // ------------------------------------------------------------
    // Editor (paper)
    // ------------------------------------------------------------
    gfx_fill_rect(0, top, client.w, view_h, 0xEAEAEA);

    gfx_fill_rect(ex, ey, ew, eh, 0xFFFFFF);
    uint32_t border = d->editor_focus ? 0x3A7AFE : 0xA0A0A0;
    gfx_draw_rect(ex, ey, ew, eh, border);

    gfx_draw_text_utf8(ex + 8, ey + 8, 0x707070, "Kuvix Note UI prototipi");
    gfx_draw_text_utf8(ex + 8, ey + 24, 0x707070, "Metin motoru sonra eklenecek...");

    // Demo caret
    if (d->editor_focus && d->caret_visible) {
        int cx = ex + 8;
        int cy = ey + 44;
        gfx_fill_rect(cx, cy, 1, 14, 0x000000);
    }

    // ------------------------------------------------------------
    // Status bar
    // ------------------------------------------------------------
    if (d->status_visible) {
        int sy = client.h - STATUS_H;
        gfx_fill_rect(0, sy, client.w, STATUS_H, 0xD6D6D6);
        gfx_draw_rect(0, sy, client.w, STATUS_H, 0xA0A0A0);

        gfx_draw_text_utf8(8, sy + 3, 0x303030, "Ln 1, Col 1");
        gfx_draw_text_utf8(client.w - 140, sy + 3, 0x303030, "UTF-8");
        gfx_draw_text_utf8(client.w - 80,  sy + 3, 0x303030, "INS");
    }

    // ------------------------------------------------------------
    // Dropdown (client-relative) - senin stilin
    // ------------------------------------------------------------
    if (d->menu_open) {
        int m_x = FILE_BTN_X;
        int m_y = MENUBAR_H;
        int m_w = 120;
        int item_h = 18;
        int m_h = 8 + (4 * item_h);

        gfx_fill_rect(m_x, m_y, m_w, m_h, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, m_w, m_h, 0x000000);

        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 4 + (i * item_h);

            if (pt_in(lx, ly, m_x, item_y, m_w, item_h)) {
                gfx_fill_rect(m_x + 1, item_y, m_w - 2, item_h, 0x000080);
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0xFFFFFF, demo_menu_items[i]);
            } else {
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0x000000, demo_menu_items[i]);
            }
        }
    }
}

static void notepad_demo_on_mouse(app_t* self, int mx, int my,
                                 uint8_t buttons, uint8_t extra1, uint8_t extra2)
{
    (void)extra1; (void)extra2;

    if (!self || !self->user) return;
    notepad_demo_t* d = (notepad_demo_t*)self->user;

    // Capture kontrolü (Notepad’deki gibi)
    if (wm_is_any_window_captured()) {
        int cap = wm_get_captured_window_id();
        if (cap != self->win_id && cap != -1) return;
    }

    ui_rect_t client = wm_get_client_rect(self->win_id);

    int lx, ly;
    mouse_to_client(&client, mx, my, &lx, &ly);

    if (lx < 0 || ly < 0 || lx >= client.w || ly >= client.h) return;

    // only left press edge
    static uint8_t prev_buttons = 0;
    uint8_t pressed = (uint8_t)(buttons & ~prev_buttons);
    prev_buttons = buttons;
    if (!(pressed & 1)) return;

    const int MENUBAR_H = 22;
    const int TOOLBAR_H = 26;
    const int STATUS_H  = 20;

    const int FILE_BTN_X = 8;
    const int FILE_BTN_Y = 3;
    const int FILE_BTN_W = 55;
    const int FILE_BTN_H = 16;

    // 1) File button
    if (pt_in(lx, ly, FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H)) {
        d->menu_open = !d->menu_open;
        return;
    }

    // 2) Dropdown açıkken seçim
    if (d->menu_open) {
        int m_x = FILE_BTN_X;
        int m_y = MENUBAR_H;
        int m_w = 120;
        int item_h = 18;
        int m_h = 8 + (4 * item_h);

        if (pt_in(lx, ly, m_x, m_y, m_w, m_h)) {
            int item = (ly - (m_y + 4)) / item_h; // 0..3
            d->menu_open = false;

            // TODO: aksiyonlar (şimdilik prototip)
            // 0 Yeni, 1 Aç, 2 Kaydet, 3 Kapat
            if (item == 3) {
                // Demo: direkt kapat
                wm_close_window(self->win_id);
                return;
            }
            // İstersen messagebox_show("...", "Not implemented") bağlarsın.
            return;
        } else {
            // dışarı tıklandı: kapat
            d->menu_open = false;
        }
    }

    // 3) Toolbar butonları (demo)
    const int TB_Y = MENUBAR_H + 4;
    const int TB_BTN_H = 18;

    int tb_x1 = 8, tb_w1 = 42;
    int tb_x2 = tb_x1 + tb_w1 + 6, tb_w2 = 42;
    int tb_x3 = tb_x2 + tb_w2 + 6, tb_w3 = 60;

    if (pt_in(lx, ly, tb_x1, TB_Y, tb_w1, TB_BTN_H)) {
        // TODO: Yeni
        return;
    }
    if (pt_in(lx, ly, tb_x2, TB_Y, tb_w2, TB_BTN_H)) {
        // TODO: Aç
        return;
    }
    if (pt_in(lx, ly, tb_x3, TB_Y, tb_w3, TB_BTN_H)) {
        // TODO: Kaydet
        return;
    }

    // 4) Editor focus
    int top = MENUBAR_H + TOOLBAR_H;
    int bottom = d->status_visible ? STATUS_H : 0;
    int view_h = client.h - top - bottom;

    int pad = 10;
    int ex = pad;
    int ey = top + pad;
    int ew = client.w - pad * 2;
    int eh = view_h - pad * 2;

    d->editor_focus = pt_in(lx, ly, ex, ey, ew, eh);

    // focus alındığında caret reset
    if (d->editor_focus) {
        d->caret_visible = 1;
        d->caret_blink_ms = 0;
        d->caret_last_ms = g_ticks_ms;
    }
}

static void notepad_demo_on_key(app_t* self, uint16_t scancode) {
    (void)self;
    (void)scancode;
    // Prototip: şimdilik boş
}

static void notepad_demo_on_destroy(app_t* self) {
    (void)self;
}

static int notepad_demo_on_close_request(app_t* self) {
    (void)self;
    // demo: kapatmaya izin ver
    return 1;
}

// ------------------------------------------------------------
// VTABLE (Notepad ile aynı stil)
// ------------------------------------------------------------
const app_vtbl_t notepad_demo_vtbl = {
    .on_create        = notepad_demo_on_create,
    .on_draw          = notepad_demo_on_draw,
    .on_key           = notepad_demo_on_key,
    .on_mouse         = notepad_demo_on_mouse,
    .on_destroy       = notepad_demo_on_destroy,
    .on_close_request = notepad_demo_on_close_request
};