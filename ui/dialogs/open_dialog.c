// ui/dialogs/open_dialog.c

#include <ui/dialogs/open_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/user.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <ui/notification.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/printk.h>

#include <ui/desktop.h>

extern void desktop_icons_reset_selection(void);
extern void desktop_reset_selection_state(void);
extern uint32_t g_ticks_ms;

static uint32_t g_last_click_ms = 0;
static int      g_last_click_idx = -1;

#ifndef DBLCLICK_MS
#define DBLCLICK_MS 350
#endif

#define MAX_ITEMS 14

typedef struct {
    char name[32];
    bool is_dir;
} dialog_item_t;

static open_dialog_t g_dlg;
static bool g_active = false;
static char g_path[128] = "/";

static dialog_item_t g_items[MAX_ITEMS];
static int g_item_count = 0;
static int g_selected = -1;

// klasör seçici modu
static bool g_pick_dir_mode = false;

// yeni klasör mini modal (şimdilik kullanılmıyor ama durabilir)
static bool g_newdir_mode = false;
static char g_newdir_buf[32];

// scroll state
static int g_scroll = 0;               // pixel
static const int g_row_h = 18;         // satır yüksekliği
static bool g_scroll_drag = false;
static int  g_scroll_drag_off = 0;

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void draw_btn(int x, int y, int w, int h, const char* t, bool hov, bool enabled) {
    (void)hov;
    uint32_t bg = enabled ? 0xAAAAAA : 0x888888;
    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, enabled ? 0x444444 : 0x666666);
    gfx_draw_text_utf8(x + 6, y + 4, 0x000000, t ? t : "");
}

static bool selected_is_dir(void) {
    if (g_selected < 0 || g_selected >= g_item_count) return false;
    return g_items[g_selected].is_dir;
}

// dx,dy,dw,dh = dialog rect
// lx,ly,lw,lh = list rect
static void list_metrics(int* out_dx, int* out_dy, int* out_dw, int* out_dh,
                         int* out_lx, int* out_ly, int* out_lw, int* out_lh)
{
    int dw = 400, dh = 310;
    int dx = (fb_get_width()  - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;

    int list_y = dy + 60;
    int list_h = 130;

    int lx = dx + 15;
    int ly = list_y;
    int lw = dw - 30;
    int lh = list_h;

    if (out_dx) *out_dx = dx;
    if (out_dy) *out_dy = dy;
    if (out_dw) *out_dw = dw;
    if (out_dh) *out_dh = dh;

    if (out_lx) *out_lx = lx;
    if (out_ly) *out_ly = ly;
    if (out_lw) *out_lw = lw;
    if (out_lh) *out_lh = lh;
}

static int get_scroll_max(int list_h) {
    int content_h = g_item_count * g_row_h;
    int max = content_h - list_h;
    if (max < 0) max = 0;
    return max;
}

static void scroll_clamp(int list_h) {
    g_scroll = clampi(g_scroll, 0, get_scroll_max(list_h));
}

// ✅ full dialog damage
static void open_dialog_damage_full(void) {
    int dx, dy, dw, dh;
    list_metrics(&dx, &dy, &dw, &dh, 0,0,0,0);

    const int PAD = 6;
    desktop_damage_rect(dx - PAD, dy - PAD, dw + PAD * 2, dh + PAD * 2);
}

// ✅ sadece list area damage (scroll/seçim için daha ucuz)
static void open_dialog_damage_list(void) {
    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    const int PAD = 4;
    desktop_damage_rect(lx - PAD, ly - PAD, lw + PAD * 2, lh + PAD * 2);
}

// ✅ sadece input satırı (dosya adı) damage
static void open_dialog_damage_input(void) {
    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    int input_y = ly + lh + 15;

    // input rect: dx+90 .. (dw-190) genişlik, 20 yükseklik
    const int ix = dx + 90;
    const int iw = dw - 190;
    const int ih = 20;

    const int PAD = 4;
    desktop_damage_rect(ix - PAD, input_y - PAD, iw + PAD * 2, ih + PAD * 2);
}

static void draw_scrollbar(int lx, int ly, int lw, int lh) {
    int content_h = g_item_count * g_row_h;
    if (content_h <= lh) return; // scroll yok

    int track_w = 8;
    int track_x = lx + lw - track_w - 2;
    int track_y = ly + 2;
    int track_h = lh - 4;

    gfx_fill_rect(track_x, track_y, track_w, track_h, 0xE0E0E0);

    int knob_h = (track_h * lh) / content_h;
    if (knob_h < 14) knob_h = 14;

    int max_scroll = get_scroll_max(lh);
    int knob_y = track_y;

    if (max_scroll > 0) {
        int travel = track_h - knob_h;
        if (travel < 1) travel = 1;
        knob_y = track_y + (g_scroll * travel) / max_scroll;
    }

    gfx_fill_rect(track_x, knob_y, track_w, knob_h, 0xA0A0A0);
}

// ------------------------------------------------------------
// VFS list callback: sadece g_path içindeki 1 seviye
// DIR tespiti: size==0 && '.' yok => dir (stat yokken idare)
// ------------------------------------------------------------
static int open_dialog_vfs_cb(const char* path, uint32_t size, void* u) {
    (void)u;

    if (g_item_count >= MAX_ITEMS) return 0;
    if (!path || !path[0]) return 1;

    // g_path item'ini listeleme
    if (strcmp(path, g_path) == 0) return 1;

    int cp_len = (int)strlen(g_path);

    // prefix değilse skip
    if (strncmp(path, g_path, cp_len) != 0) return 1;

    // "/homeX" gibi yanlış prefixleri engelle
    if (path[cp_len] != '\0' && path[cp_len] != '/') return 1;

    const char* rel = path + cp_len;
    if (rel[0] == '/') rel++;

    if (rel[0] == '\0') return 1;

    // sadece 1 seviye
    if (strchr(rel, '/') != NULL) return 1;

    strncpy(g_items[g_item_count].name, rel, 31);
    g_items[g_item_count].name[31] = '\0';

    bool has_dot = (strchr(rel, '.') != NULL);
    g_items[g_item_count].is_dir = (size == 0 && !has_dot);

    g_item_count++;
    return 1;
}

void open_dialog_refresh(void) {
    g_item_count = 0;
    g_selected = -1;
    memset(g_items, 0, sizeof(g_items));

    vfs_list(g_path, open_dialog_vfs_cb, NULL);

    g_last_click_idx = -1;
    g_last_click_ms = 0;

    g_scroll = 0;
    g_scroll_drag = false;
    g_scroll_drag_off = 0;

    // ✅ dialog içeriği değişti
    open_dialog_damage_full();
}

static void open_dialog_enter_dir(const char* name) {
    if (!name || !name[0]) return;

    char newp[128];
    memset(newp, 0, sizeof(newp));

    if (strcmp(g_path, "/") == 0) {
        strcpy(newp, "/");
        strncat(newp, name, sizeof(newp) - strlen(newp) - 1);
    } else {
        strncpy(newp, g_path, sizeof(newp) - 1);
        if (newp[strlen(newp) - 1] != '/') {
            strncat(newp, "/", sizeof(newp) - strlen(newp) - 1);
        }
        strncat(newp, name, sizeof(newp) - strlen(newp) - 1);
    }

    strncpy(g_path, newp, sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';

    g_selected = -1;
    g_dlg.buffer[0] = '\0';

    open_dialog_refresh(); // ✅ refresh zaten full damage yapıyor
}

static void open_dialog_go_back(void) {
    if (strcmp(g_path, "/") == 0) return;

    char* last = strrchr(g_path, '/');
    if (!last) return;

    if (last == g_path) g_path[1] = '\0';
    else *last = '\0';

    g_selected = -1;
    g_dlg.buffer[0] = '\0';

    open_dialog_refresh();
}

static void open_dialog_do_open_file(void) {
    if (!g_dlg.on_open) { g_active = false; return; }

    if (strlen(g_dlg.buffer) == 0) {
        notification_show("Dosya sec!", 1500);
        return;
    }

    char full[256];
    memset(full, 0, sizeof(full));
    strcpy(full, g_path);
    int len = (int)strlen(full);
    if (len > 0 && full[len - 1] != '/') strcat(full, "/");
    strcat(full, g_dlg.buffer);

    // kapanırken son görüntüyü sildirmek için
    open_dialog_damage_full();

    g_active = false;
    g_dlg.on_open(full);
}

static void open_dialog_do_pick_dir(void) {
    if (!g_dlg.on_open) { g_active = false; return; }

    // seçili varsa
    if (g_selected >= 0 && g_selected < g_item_count) {
        if (!g_items[g_selected].is_dir) {
            notification_show("Klasor secmelisin!", 1500);
            return;
        }

        char full[256];
        memset(full, 0, sizeof(full));
        strcpy(full, g_path);
        int len = (int)strlen(full);
        if (len > 0 && full[len - 1] != '/') strcat(full, "/");
        strcat(full, g_items[g_selected].name);

        open_dialog_damage_full();
        g_active = false;
        g_dlg.on_open(full);
        return;
    }

    // hiç seçim yoksa: bulunduğun klasörü seç
    open_dialog_damage_full();
    g_active = false;
    g_dlg.on_open(g_path);
}

// ------------------------------------------------------------
// Wheel API (desktop tick'ten çağır)
// step: +1/-1
// ------------------------------------------------------------
void open_dialog_handle_wheel(int step) {
    if (!g_active) return;

    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    int old = g_scroll;

    // 2 satır gibi
    g_scroll += step * 36;
    scroll_clamp(lh);

    if (g_scroll != old) {
        open_dialog_damage_list();
    }
}

// ------------------------------------------------------------
// Mouse move API (drag scroll için)
// ------------------------------------------------------------
void open_dialog_handle_mouse_move(int mx, int my, uint8_t btns) {
    if (!g_active) return;

    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    int content_h = g_item_count * g_row_h;
    if (content_h <= lh) { g_scroll_drag = false; return; }

    // LMB bırakıldıysa drag bitir
    if (!(btns & 1)) { g_scroll_drag = false; return; }
    if (!g_scroll_drag) return;

    int track_w = 8;
    int track_x = lx + lw - track_w - 2;
    int track_y = ly + 2;
    int track_h = lh - 4;

    int knob_h = (track_h * lh) / content_h;
    if (knob_h < 14) knob_h = 14;

    int max_scroll = get_scroll_max(lh);
    int travel = track_h - knob_h;
    if (travel <= 0) return;

    int old = g_scroll;

    int knob_y = my - g_scroll_drag_off;
    int target = knob_y - track_y;
    target = clampi(target, 0, travel);

    if (max_scroll > 0) g_scroll = (target * max_scroll) / travel;
    else g_scroll = 0;

    scroll_clamp(lh);

    if (g_scroll != old) {
        open_dialog_damage_list();
    }

    (void)track_x;
}

// ------------------------------------------------------------
// INPUT (click/press)
// ------------------------------------------------------------
void open_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!g_active) return;

    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    if (!clicked) return;

    // X
    if (mx >= dx + dw - 22 && mx <= dx + dw - 4 &&
        my >= dy + 4 && my <= dy + 20) {
        open_dialog_damage_full(); // kapanınca temizlensin
        g_active = false;
        g_newdir_mode = false;
        g_scroll_drag = false;
        return;
    }

    int nav_y = dy + 30;

    // back
    if (mx >= dx + 15 && mx <= dx + 37 &&
        my >= nav_y && my <= nav_y + 22) {
        if (!g_newdir_mode) open_dialog_go_back(); // refresh + damage
        return;
    }

    // ------------------------------------------------------------
    // LIST: scrollbar click / list click
    // ------------------------------------------------------------
    {
        int content_h = g_item_count * g_row_h;
        if (content_h > lh) {
            int track_w = 8;
            int track_x = lx + lw - track_w - 2;
            int track_y = ly + 2;
            int track_h = lh - 4;

            if (hit(mx, my, track_x, track_y, track_w, track_h)) {
                int max_scroll = get_scroll_max(lh);

                int knob_h = (track_h * lh) / content_h;
                if (knob_h < 14) knob_h = 14;

                int travel = track_h - knob_h;
                if (travel < 1) travel = 1;

                int knob_y = track_y;
                if (max_scroll > 0) knob_y = track_y + (g_scroll * travel) / max_scroll;

                // knob içine tıklarsa drag başlat
                if (my >= knob_y && my <= knob_y + knob_h) {
                    g_scroll_drag = true;
                    g_scroll_drag_off = my - knob_y;
                    return;
                }

                // track'e tık: o noktaya zıplat
                int old = g_scroll;

                int target = my - track_y - knob_h / 2;
                target = clampi(target, 0, travel);
                g_scroll = (max_scroll > 0) ? (target * max_scroll) / travel : 0;
                scroll_clamp(lh);

                if (g_scroll != old) open_dialog_damage_list();
                return;
            }
        }
    }

    // list area click -> index hesapla (scroll dahil)
    if (mx >= lx && mx <= lx + lw && my >= ly && my <= ly + lh) {
        int inner_y = my - (ly + 4);
        int y_scrolled = inner_y + g_scroll;
        int idx = y_scrolled / g_row_h;

        if (idx >= 0 && idx < g_item_count) {
            uint32_t now = g_ticks_ms;
            bool is_dbl = (g_last_click_idx == idx) && ((now - g_last_click_ms) < DBLCLICK_MS);

            int old_sel = g_selected;

            g_selected = idx;
            g_last_click_idx = idx;
            g_last_click_ms = now;

            // seçim değiştiyse list’i yenile
            if (g_selected != old_sel) {
                open_dialog_damage_list();
                if (!g_pick_dir_mode) open_dialog_damage_input(); // dosya adı alanı da değişebilir
            }

            if (g_items[idx].is_dir) {
                g_dlg.buffer[0] = '\0';
                if (!g_pick_dir_mode) open_dialog_damage_input();

                if (is_dbl) {
                    open_dialog_enter_dir(g_items[idx].name);
                }
            } else {
                if (!g_pick_dir_mode) {
                    strncpy(g_dlg.buffer, g_items[idx].name, 63);
                    g_dlg.buffer[63] = '\0';
                    open_dialog_damage_input();
                }

                if (is_dbl) {
                    open_dialog_do_open_file();
                }
            }
        }
        return;
    }

    // ------------------------------------------------------------
    // BUTTONS
    // ------------------------------------------------------------
    int input_y = (dy + 60) + 130 + 15;
    int btn_x = dx + dw - 85;

    // ana buton
    if (hit(mx, my, btn_x, input_y, 70, 22)) {
        if (g_pick_dir_mode) {
            if (g_selected >= 0 && g_selected < g_item_count && !g_items[g_selected].is_dir) {
                notification_show("Klasor secmelisin!", 1500);
                return;
            }
            open_dialog_do_pick_dir();
        } else {
            open_dialog_do_open_file();
        }
        return;
    }

    // iptal
    if (hit(mx, my, btn_x, input_y + 30, 70, 22)) {
        open_dialog_damage_full();
        g_active = false;
        g_scroll_drag = false;
        return;
    }
}

// ------------------------------------------------------------
// DRAW
// ------------------------------------------------------------
void open_dialog_draw(void) {
    if (!g_active) return;

    int dx, dy, dw, dh, lx, ly, lw, lh;
    list_metrics(&dx, &dy, &dw, &dh, &lx, &ly, &lw, &lh);

    int btn_x = dx + dw - 85;

    gfx_fill_rect(dx, dy, dw, dh, 0xC6C6C6);
    gfx_draw_rect(dx, dy, dw, dh, 0x000000);

    gfx_fill_rect(dx + 2, dy + 2, dw - 4, 20, 0x000080);
    gfx_draw_text_utf8(dx + 8, dy + 5, 0xFFFFFF, g_dlg.title);

    // X
    gfx_fill_rect(dx + dw - 22, dy + 4, 18, 16, 0xFF0000);
    gfx_draw_text_utf8(dx + dw - 16, dy + 6, 0xFFFFFF, "X");

    // path
    int nav_y = dy + 30;
    gfx_fill_rect(dx + 15, nav_y, 22, 22, 0xAAAAAA);
    gfx_draw_text_utf8(dx + 22, nav_y + 4, 0x000000, "<");

    gfx_fill_rect(dx + 42, nav_y, dw - 95, 22, 0xFFFFFF);
    gfx_draw_rect(dx + 42, nav_y, dw - 95, 22, 0x808080);
    gfx_draw_text_utf8(dx + 47, nav_y + 4, 0x000000, g_path);

    // list box
    gfx_fill_rect(lx, ly, lw, lh, 0xFFFFFF);
    gfx_draw_rect(lx, ly, lw, lh, 0x808080);

    scroll_clamp(lh);

    // only visible rows
    int first = g_scroll / g_row_h;
    int y_off = -(g_scroll % g_row_h);

    int visible_rows = (lh / g_row_h) + 2;
    int last = first + visible_rows;
    if (last > g_item_count) last = g_item_count;

    for (int i = first; i < last; i++) {
        int iy = ly + 4 + y_off + (i - first) * g_row_h;

        // clipping
        if (iy + g_row_h < ly) continue;
        if (iy > ly + lh) break;

        if (g_selected == i) gfx_fill_rect(lx + 1, iy, lw - 2, g_row_h - 1, 0xCCE8FF);

        uint32_t color = g_items[i].is_dir ? 0x0000AA : 0x000000;
        gfx_draw_text_utf8(lx + 5,  iy + 2, color, g_items[i].is_dir ? ">" : "-");
        gfx_draw_text_utf8(lx + 20, iy + 2, color, g_items[i].name);
    }

    // scrollbar
    draw_scrollbar(lx, ly, lw, lh);

    // input area
    int input_y = ly + lh + 15;
    gfx_draw_text_utf8(dx + 15, input_y + 3, 0x000000, g_pick_dir_mode ? "Secim:" : "Dosya:");

    gfx_fill_rect(dx + 90, input_y, dw - 190, 20, g_pick_dir_mode ? 0xEEEEEE : 0xFFFFFF);
    gfx_draw_rect(dx + 90, input_y, dw - 190, 20, 0x808080);

    if (!g_pick_dir_mode) {
        gfx_draw_text_utf8(dx + 95, input_y + 3, 0x000000, g_dlg.buffer);
    } else {
        if (g_selected >= 0 && g_selected < g_item_count) {
            gfx_draw_text_utf8(dx + 95, input_y + 3, 0x000000, g_items[g_selected].name);
        } else {
            gfx_draw_text_utf8(dx + 95, input_y + 3, 0x000000, "(mevcut klasor)");
        }
    }

    // buttons
    const char* main_text = g_pick_dir_mode ? "Klasoru Sec" : "Ac";
    bool main_enabled = (!g_pick_dir_mode) ? true : (g_selected < 0 || selected_is_dir());

    draw_btn(btn_x, input_y, 70, 22, main_text, false, main_enabled);
    draw_btn(btn_x, input_y + 30, 70, 22, "Iptal", false, true);
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------
void open_dialog_show(const char* title, const char* initial_name, int owner_win_id, open_callback_t cb) {
    memset(&g_dlg, 0, sizeof(g_dlg));

    strncpy(g_dlg.title, title ? title : "Dosya Ac", 31);
    g_dlg.title[31] = '\0';

    g_pick_dir_mode = false;

    if (initial_name && initial_name[0]) {
        strncpy(g_dlg.buffer, initial_name, 63);
        g_dlg.buffer[63] = '\0';
    } else {
        g_dlg.buffer[0] = '\0';
    }

    g_dlg.on_open = cb;
    g_dlg.owner_win_id = owner_win_id;

    g_active = true;

    desktop_reset_selection_state();
    desktop_icons_reset_selection();

    strcpy(g_path, USER_DESKTOP_PATH);
    open_dialog_refresh(); // refresh -> full damage
}

void open_dialog_show_dirpicker(const char* title, const char* initial_path, int owner_win_id, open_callback_t cb) {
    open_dialog_show(title, "", owner_win_id, cb);
    g_pick_dir_mode = true;

    if (initial_path && initial_path[0]) {
        strncpy(g_path, initial_path, sizeof(g_path) - 1);
        g_path[sizeof(g_path) - 1] = '\0';
    }

    g_dlg.buffer[0] = '\0';
    open_dialog_refresh();
}

bool open_dialog_is_active(void) { return g_active; }

int open_dialog_get_owner_win_id(void) {
    return g_dlg.owner_win_id;
}

void open_dialog_handle_key(uint16_t scancode, char c) {
    if (!g_active) return;

    // Esc
    if (scancode == 0x01) {
        open_dialog_damage_full();
        g_active = false;
        g_newdir_mode = false;
        g_scroll_drag = false;
        return;
    }

    // Enter
    if (scancode == 0x1C) {
        if (g_pick_dir_mode) open_dialog_do_pick_dir();
        else open_dialog_do_open_file();
        return;
    }

    // klasör seçim modunda yazı yazdırma yok
    if (g_pick_dir_mode) return;

    // Backspace
    if (c == '\b') {
        int len = (int)strlen(g_dlg.buffer);
        if (len > 0) {
            g_dlg.buffer[len - 1] = '\0';
            open_dialog_damage_input();
        }
        return;
    }

    // normal karakter
    if (c >= 32 && c <= 126) {
        int len = (int)strlen(g_dlg.buffer);
        if (len < 63) {
            g_dlg.buffer[len] = c;
            g_dlg.buffer[len + 1] = '\0';
            open_dialog_damage_input();
        }
        return;
    }
}