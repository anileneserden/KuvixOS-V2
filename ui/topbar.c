// ui/topbar.c
#include <ui/topbar.h>
#include <ui/desktop.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>

// --- net panel/status ---
#include <ui/net_status.h>
#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/e1000.h>

#include <kernel/drivers/rtc/rtc.h>
#include <kernel/time.h>

static int bar_h = 28;
static int btn_w = 120;   // pencere buton genişliği
static int btn_h = 22;

// net dropdown
static int g_net_panel_open = 0;

// ------------------------------------------------------------
// small helpers (no snprintf)
// ------------------------------------------------------------
static int u32_append_dec(char* out, int cap, int* io_len, uint32_t v) {
    char tmp[16];
    int n = 0;

    if (v == 0) tmp[n++] = '0';
    else {
        while (v && n < 10) {
            tmp[n++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }

    for (int i = n - 1; i >= 0; i--) {
        if (*io_len >= cap - 1) { out[cap - 1] = 0; return 0; }
        out[(*io_len)++] = tmp[i];
    }
    out[*io_len] = 0;
    return 1;
}

static void ip_to_str(uint32_t ip_be, char out[32]) {
    int p = 0;
    out[0] = 0;

    u32_append_dec(out, 32, &p, (ip_be >> 24) & 0xFF); if (p < 31) out[p++]='.';
    u32_append_dec(out, 32, &p, (ip_be >> 16) & 0xFF); if (p < 31) out[p++]='.';
    u32_append_dec(out, 32, &p, (ip_be >> 8)  & 0xFF); if (p < 31) out[p++]='.';
    u32_append_dec(out, 32, &p, (ip_be >> 0)  & 0xFF);

    out[p] = 0;
}

static int pt_in(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

// ------------------------------------------------------------
// window buttons
// ------------------------------------------------------------
static void draw_window_buttons(void) {
    int count = wm_get_count();
    int active = wm_get_active_id();
    int x = 120; // logo sonrası başlasın

    for (int zi = 0; zi < count; zi++) {
        int id = wm_get_z(zi);
        const ui_window_t* w = wm_get_window_ptr(id);
        if (!w) continue;

        uint32_t bg = 0x222222;
        uint32_t fg = 0xCCCCCC;

        if (id == active) {
            bg = 0x00AAFF;
            fg = 0xFFFFFF;
        } else if (w->state == WIN_MINIMIZED) {
            bg = 0x1A1A1A;
            fg = 0x777777;
        }

        gfx_fill_rect(x, 3, btn_w, btn_h, bg);

        char title_buf[32];
        strncpy(title_buf, w->title ? w->title : "Window", 30);
        title_buf[30] = 0;

        gfx_draw_text(x + 8, 8, fg, title_buf);

        x += btn_w + 6;
    }
}

// ------------------------------------------------------------
// NET button + dropdown
// ------------------------------------------------------------
static void net_button_rect(int sw, int* ox, int* oy, int* ow, int* oh) {
    // sağ tarafta, saat metninden önce
    int w = 22;
    int h = 22;
    int x = sw - 120 - 10 - w; // "17:11  CPU..." alanından önce
    int y = 3;
    *ox = x; *oy = y; *ow = w; *oh = h;
}

static uint32_t net_color(net_state_t st) {
    if (st == NETS_INET) return 0x00FF00;   // yeşil
    if (st == NETS_LAN)  return 0xFF6A00;   // turuncu
    if (st == NETS_LINK) return 0xC0C0C0;   // açık gri
    return 0x666666;                        // kapalı
}

static void draw_net_button(int sw) {
    int x,y,w,h;
    net_button_rect(sw, &x, &y, &w, &h);

    // button bg/border
    gfx_fill_rect(x, y, w, h, 0x1B1B1B);
    gfx_draw_rect(x, y, w, h, g_net_panel_open ? 0xFF6A00 : 0x404040);

    // status dot
    net_state_t st = net_status_get();
    uint32_t c = net_color(st);

    int cx = x + w/2;
    int cy = y + h/2;

    // basit "dot": küçük 6x6 kare (circle yoksa)
    gfx_fill_rect(cx - 3, cy - 3, 6, 6, c);
}

static void draw_net_menu(int sw) {
    if (!g_net_panel_open) return;

    int bx, by, bw, bh;
    net_button_rect(sw, &bx, &by, &bw, &bh);

    // menu box: butonun altına doğru
    int w = 280;
    int h = 160;
    int x = bx + bw - w;      // sağa hizala
    if (x < 8) x = 8;
    int y = by + bh + 6;

    gfx_fill_rect(x, y, w, h, 0x101010);
    gfx_draw_rect(x, y, w, h, 0x404040);

    // header
    gfx_draw_text(x + 10, y + 10, 0xFFFFFF, "Network");
    gfx_draw_text(x + 10, y + 28, 0xAAAAAA, net_status_text());

    uint32_t ip=0, mask=0, gw=0;
    net_get_ipv4(&ip, &mask, &gw);

    char ip_s[32], mask_s[32], gw_s[32];
    ip_to_str(ip, ip_s);
    ip_to_str(mask, mask_s);
    ip_to_str(gw, gw_s);

    gfx_draw_text(x + 10, y + 50, 0xCCCCCC, "IP:");
    gfx_draw_text(x + 70, y + 50, 0xFFFFFF, ip_s);

    gfx_draw_text(x + 10, y + 66, 0xCCCCCC, "MASK:");
    gfx_draw_text(x + 70, y + 66, 0xFFFFFF, mask_s);

    gfx_draw_text(x + 10, y + 82, 0xCCCCCC, "GW:");
    gfx_draw_text(x + 70, y + 82, 0xFFFFFF, gw_s);

    // Buttons row
    // Test
    gfx_fill_rect(x + 10, y + 110, 90, 22, 0x202020);
    gfx_draw_rect(x + 10, y + 110, 90, 22, 0x505050);
    gfx_draw_text(x + 18, y + 116, 0xFFFFFF, "Test");

    // Disconnect (placeholder)
    gfx_fill_rect(x + 110, y + 110, 120, 22, 0x202020);
    gfx_draw_rect(x + 110, y + 110, 120, 22, 0x505050);
    gfx_draw_text(x + 118, y + 116, 0xFFFFFF, "Disconnect");

    // küçük hint
    gfx_draw_text(x + 10, y + 140, 0x777777, "Tip: Test ile ping kontrol edilir");
}

// ------------------------------------------------------------
// public
// ------------------------------------------------------------
void topbar_init(void) {
    net_status_init();
}

void topbar_handle_mouse(int mx, int my) {
    int sw = fb_get_width();

    // 1) NET butonu (bar üstünde)
    int bx, by, bw, bh;
    net_button_rect(sw, &bx, &by, &bw, &bh);

    if (pt_in(mx, my, bx, by, bw, bh)) {
        g_net_panel_open ^= 1;
        net_status_force_check();
        desktop_request_redraw();
        return;
    }

    // 2) Menü açıksa: menü clickleri ve dışarı tıkla kapat
    if (g_net_panel_open) {
        int w = 280;
        int h = 160;
        int x = bx + bw - w; if (x < 8) x = 8;
        int y = by + bh + 6;

        // Test button
        if (pt_in(mx, my, x + 10, y + 110, 90, 22)) {
            net_status_force_check();
            return;
        }

        // Disconnect placeholder
        if (pt_in(mx, my, x + 110, y + 110, 120, 22)) {
            // Şimdilik sadece kapatıyoruz; ileride net_enabled=0 yaparız
            g_net_panel_open = 0;
            return;
        }

        // Menü dışına tık => kapat (consume)
        if (!pt_in(mx, my, x, y, w, h)) {
            g_net_panel_open = 0;
            return;
        }

        // Menü içi başka yere tık => consume
        return;
    }

    // 3) Bar dışı => window buttons yok
    if (my >= bar_h) return;

    // 4) Window buttons click
    int count = wm_get_count();
    int active = wm_get_active_id();

    int x = 120;

    for (int zi = 0; zi < count; zi++) {
        int id = wm_get_z(zi);
        const ui_window_t* wptr = wm_get_window_ptr(id);
        if (!wptr) continue;

        if (mx >= x && mx < x + btn_w && my >= 3 && my < 3 + btn_h) {
            if (wptr->state == WIN_MINIMIZED) {
                wm_restore(id);
            } else if (id == active) {
                wm_minimize(id);
            } else {
                wm_set_active(id);
            }
            return;
        }

        x += btn_w + 6;
    }
}

static uint8_t s_last_min = 255;
static uint8_t s_last_sec = 255;

static uint32_t s_last_sec_bucket = 0xFFFFFFFF;

static int s_topbar_dirty = 1;

int topbar_consume_dirty(void) {
    int d = s_topbar_dirty;
    s_topbar_dirty = 0;
    return d;
}

void topbar_tick(void) {
    uint32_t sec = (uint32_t)time_now_epoch_sec(); // veya local
    static uint32_t last = 0xFFFFFFFF;
    if (sec != last) {
        last = sec;
        s_topbar_dirty = 1;
        desktop_request_redraw(); // sahneyi çizdir
    }
}

void topbar_draw(void) {
    int sw = fb_get_width();

    // Arkaplan
    gfx_fill_rect(0, 0, sw, bar_h, 0x111111);

    // Alt çizgi
    gfx_fill_rect(0, bar_h - 1, sw, 1, 0x00AAFF);

    // Logo
    gfx_draw_text(15, 7, 0xFFFFFF, "KuvixOS");

    // Net tick (periyodik)
    net_status_tick();

    // NET kare buton
    draw_net_button(sw);

    // Açık pencereler
    draw_window_buttons();

    // Sağ saat
    gfx_draw_text(sw - 120, 7, 0xAAAAAA, "17:11  CPU: 2%");
}