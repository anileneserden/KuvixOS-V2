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

// ✅ klasör seçici modu
static bool g_pick_dir_mode = false;

// yeni klasör mini modal
static bool g_newdir_mode = false;
static char g_newdir_buf[32];

static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

static void draw_btn(int x, int y, int w, int h, const char* t, bool hov, bool enabled) {
    uint32_t bg = enabled ? 0xAAAAAA : 0x888888;
    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, hov ? 0xFFFFFF : 0x444444);
    gfx_draw_text_utf8(x + 6, y + 4, 0x000000, t);
}

static bool selected_is_dir(void) {
    if (g_selected < 0 || g_selected >= g_item_count) return false;
    return g_items[g_selected].is_dir;
}

// ------------------------------------------------------------
// VFS list callback: sadece g_path içindeki 1 seviye
// DIR tespiti: size==0 && '.' yok => dir
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

    // ✅ DIR tespiti (stat yok!)
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

    // klasöre girince seçim temizle
    g_selected = -1;
    g_dlg.buffer[0] = '\0';

    open_dialog_refresh();
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

        g_active = false;
        g_dlg.on_open(full);
        return;
    }

    // hiç seçim yoksa: bulunduğun klasörü seç
    g_active = false;
    g_dlg.on_open(g_path);
}

// ------------------------------------------------------------
// INPUT
// ------------------------------------------------------------
void open_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!g_active || !clicked) return;

    int dw = 400, dh = 310;
    int dx = (fb_get_width() - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;

    // X
    if (mx >= dx + dw - 22 && mx <= dx + dw - 4 &&
        my >= dy + 4 && my <= dy + 20) {
        g_active = false;
        g_newdir_mode = false;
        return;
    }

    int nav_y = dy + 30;

    // back
    if (mx >= dx + 15 && mx <= dx + 37 &&
        my >= nav_y && my <= nav_y + 22) {
        if (!g_newdir_mode) open_dialog_go_back();
        return;
    }

    // LISTE
    int list_y = dy + 60;

    for (int i = 0; i < g_item_count; i++) {
        int iy = list_y + 4 + (i * 18);

        if (mx >= dx + 15 && mx <= dx + dw - 15 &&
            my >= iy && my <= iy + 18) {

            uint32_t now = g_ticks_ms;
            bool is_dbl = (g_last_click_idx == i) && ((now - g_last_click_ms) < DBLCLICK_MS);

            // tek tık: seç
            g_selected = i;
            g_last_click_idx = i;
            g_last_click_ms = now;

            // dosya ise buffer'a yaz, klasör ise buffer temiz
            if (g_items[i].is_dir) {
                g_dlg.buffer[0] = '\0';
                if (is_dbl) {
                    // ✅ çift tık: klasöre gir
                    open_dialog_enter_dir(g_items[i].name);
                }
            } else {
                strncpy(g_dlg.buffer, g_items[i].name, 63);
                g_dlg.buffer[63] = '\0';
            }
            return;
        }
    }

    // BUTONLAR
    int input_y = list_y + 130 + 15;
    int btn_x = dx + dw - 85;

    // ana buton
    if (hit(mx, my, btn_x, input_y, 70, 22)) {
        if (g_pick_dir_mode) {
            // dosya seçiliyse engelle
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
        g_active = false;
        return;
    }
}

// ------------------------------------------------------------
// DRAW (sadece buton label kısmı önemli)
// ------------------------------------------------------------
void open_dialog_draw(void) {
    if (!g_active) return;

    int dw = 400, dh = 310;
    int dx = (fb_get_width() - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;
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

    // list
    int list_y = dy + 60;
    int list_h = 130;
    gfx_fill_rect(dx + 15, list_y, dw - 30, list_h, 0xFFFFFF);
    gfx_draw_rect(dx + 15, list_y, dw - 30, list_h, 0x808080);

    for (int i = 0; i < g_item_count; i++) {
        int iy = list_y + 4 + (i * 18);
        if (g_selected == i) gfx_fill_rect(dx + 16, iy, dw - 32, 17, 0xCCE8FF);

        uint32_t color = g_items[i].is_dir ? 0x0000AA : 0x000000;
        gfx_draw_text_utf8(dx + 20, iy + 2, color, g_items[i].is_dir ? ">" : "-");
        gfx_draw_text_utf8(dx + 35, iy + 2, color, g_items[i].name);
    }

    // input yeri (klasör modunda da dursun ama pasif)
    int input_y = list_y + list_h + 15;
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
    open_dialog_refresh();
}

// ✅ klasör seçici aç
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
        g_active = false;
        g_newdir_mode = false;
        return;
    }

    // Enter
    if (scancode == 0x1C) {
        if (g_pick_dir_mode) {
            // klasör modunda Enter = klasörü seç
            // (seçim yoksa current klasörü seçer)
            open_dialog_do_pick_dir();
        } else {
            open_dialog_do_open_file();
        }
        return;
    }

    // klasör seçim modunda yazı yazdırma yok (input pasif)
    if (g_pick_dir_mode) return;

    // Backspace
    if (c == '\b') {
        int len = (int)strlen(g_dlg.buffer);
        if (len > 0) g_dlg.buffer[len - 1] = '\0';
        return;
    }

    // normal karakter
    if (c >= 32 && c <= 126) {
        int len = (int)strlen(g_dlg.buffer);
        if (len < 63) {
            g_dlg.buffer[len] = c;
            g_dlg.buffer[len + 1] = '\0';
        }
        return;
    }
}