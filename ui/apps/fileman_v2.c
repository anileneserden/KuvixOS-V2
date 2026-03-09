// ui/apps/fileman_v2.c
#include <ui/apps/fileman_v2.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/fs/vfs.h>
#include <kernel/user.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <ui/desktop_icons/folder_icon.h>

#include <app/app_manager.h>
#include <ui/apps/kuvix_browser.h>

extern char kbd_scancode_to_ascii(uint8_t scancode);
extern uint32_t g_ticks_ms;

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static fileman_v2_t* fm(app_t* app) {
    return (app && app->user) ? (fileman_v2_t*)app->user : NULL;
}

static uint32_t fm_ticks_ms(void) {
    return g_ticks_ms;
}

static void fm_set_status(fileman_v2_t* f, const char* s) {
    if (!f) return;
    if (!s) s = "";
    strncpy(f->status, s, sizeof(f->status) - 1);
    f->status[sizeof(f->status) - 1] = 0;
}

static const char* fm_basename(const char* path) {
    if (!path) return "";
    const char* p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static void fm_parent_dir(const char* in, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!in || !in[0]) { strncpy(out, "/", cap - 1); out[cap - 1] = 0; return; }

    strncpy(out, in, cap - 1);
    out[cap - 1] = 0;

    int n = (int)strlen(out);
    while (n > 1 && out[n - 1] == '/') { out[n - 1] = 0; n--; }

    char* p = strrchr(out, '/');
    if (!p) { strncpy(out, "/", cap - 1); out[cap - 1] = 0; return; }

    if (p == out) { out[1] = 0; return; } // "/"
    *p = 0;
}

static bool fm_is_dir(const char* path) {
    vfs_stat_t st;
    if (!vfs_stat(path, &st)) return false;
    return (st.type == VFS_T_DIR);
}

static void fm_load_dir(fileman_v2_t* f, const char* path);

static bool ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    int sl = (int)strlen(s);
    int tl = (int)strlen(suffix);
    if (tl <= 0 || sl < tl) return false;
    return (strcmp(s + (sl - tl), suffix) == 0);
}

// ------------------------------------------------------------
// vfs list callback
// ------------------------------------------------------------
static int fm_list_cb(const char* entry_path, uint32_t size, void* u) {
    (void)size;

    fileman_v2_t* f = (fileman_v2_t*)u;
    if (!f) return 0;
    if (f->item_count >= FM2_MAX_ITEMS) return 0;

    if (!entry_path || !entry_path[0]) return 1;

    const char* base = fm_basename(entry_path);

    // 1-level child filter
    int plen = (int)strlen(f->path);
    if (strncmp(entry_path, f->path, (size_t)plen) == 0) {
        const char* rest = entry_path + plen;
        if (rest[0] == '/') rest++;

        if (strchr(rest, '/')) {
            return 1;
        }
    }

    // "." ve ".." gizle
    if (!strcmp(base, ".") || !strcmp(base, "..")) {
        return 1;
    }

    // current path'i gösterme
    if (!strcmp(entry_path, f->path)) {
        return 1;
    }

    // "/home/anil" içindeyken "anil" gelmesin gibi self-child
    const char* cur_base = fm_basename(f->path);
    if (cur_base && cur_base[0] && !strcmp(base, cur_base)) {
        return 1;
    }

    fm2_item_t* it = &f->items[f->item_count];
    memset(it, 0, sizeof(*it));

    strncpy(it->full, entry_path, sizeof(it->full) - 1);
    it->full[sizeof(it->full) - 1] = 0;

    strncpy(it->name, base, sizeof(it->name) - 1);
    it->name[sizeof(it->name) - 1] = 0;

    vfs_stat_t st;
    it->is_dir = (vfs_stat(entry_path, &st) && st.type == VFS_T_DIR);

    f->item_count++;
    return 1;
}

static void fm_load_dir(fileman_v2_t* f, const char* path) {
    if (!f) return;
    if (!path || !path[0]) path = "/";

    if (!fm_is_dir(path)) {
        fm_set_status(f, "Invalid folder");
        return;
    }

    strncpy(f->path, path, sizeof(f->path) - 1);
    f->path[sizeof(f->path) - 1] = 0;

    f->item_count = 0;
    f->selected = -1;
    f->scroll_y = 0;

    vfs_list(f->path, fm_list_cb, f);
    fm_set_status(f, "Ready");
}

// ------------------------------------------------------------
// simple UI rect + hit
// ------------------------------------------------------------
typedef struct { int x, y, w, h; } rect_t;

static bool pt_in(rect_t r, int px, int py) {
    return (px >= r.x && py >= r.y && px < (r.x + r.w) && py < (r.y + r.h));
}

static rect_t r_toolbar(int cw) { return (rect_t){0, 0, cw, 34}; }
static rect_t r_content(int cw, int ch) { return (rect_t){0, 34, cw, ch - 34}; }

// toolbar controls
static rect_t r_btn_up(void)      { return (rect_t){ 8, 6, 26, 22 }; }
static rect_t r_btn_refresh(void) { return (rect_t){ 38, 6, 26, 22 }; }

static rect_t r_pathbar(int cw) {
    int x = 70;
    int y = 6;
    int w = cw - x - 8;
    if (w < 80) w = 80;
    return (rect_t){ x, y, w, 22 };
}

// grid metrics
#define CELL_W  96
#define CELL_H  110

// icon scale settings (20x20 -> 40x40)
#define ICON20_SCALE 2
#define ICON20_W (20 * ICON20_SCALE)
#define ICON20_H (20 * ICON20_SCALE)

// ------------------------------------------------------------
// icon drawing (palette 0..3) + scale
// ------------------------------------------------------------
static void draw_icon20_scaled(int x, int y,
                               const uint8_t icon[20][20],
                               uint32_t c1, uint32_t c2, uint32_t c3,
                               int scale)
{
    for (int iy = 0; iy < 20; iy++) {
        for (int ix = 0; ix < 20; ix++) {
            uint8_t v = icon[iy][ix];
            if (v == 0) continue;

            uint32_t col = (v == 1) ? c1 : (v == 2) ? c2 : c3;

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    gfx_putpixel(x + ix * scale + sx,
                                 y + iy * scale + sy,
                                 col);
                }
            }
        }
    }
}

static void draw_simple_file_icon20_scaled(int x, int y, int scale) {
    for (int iy = 0; iy < 20; iy++) {
        for (int ix = 0; ix < 20; ix++) {
            int inside = (ix >= 2 && ix <= 17 && iy >= 0 && iy <= 19);
            if (!inside) continue;

            uint32_t col = 0xFF2A2A2A;
            if (iy == 0 || iy == 19 || ix == 2 || ix == 17) col = 0xFF707070;
            if ((ix == 16 && iy == 1) || (ix == 15 && iy == 2) || (ix == 16 && iy == 2)) col = 0xFFA0A0A0;

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    gfx_putpixel(x + ix * scale + sx, y + iy * scale + sy, col);
                }
            }
        }
    }
}

// ------------------------------------------------------------
// toolbar draw
// ------------------------------------------------------------
static void draw_button(rect_t r, const char* text) {
    gfx_fill_rect(r.x, r.y, r.w, r.h, 0x202020);
    gfx_draw_rect(r.x, r.y, r.w, r.h, 0x505050);
    gfx_draw_text_utf8(r.x + 7, r.y + 6, 0xFFFFFF, text);
}

static void draw_pathbar(rect_t r, const char* text, bool edit_mode) {
    uint32_t bg = 0x101010;
    uint32_t bd = edit_mode ? 0xFF6A00 : 0x505050;
    uint32_t fg = 0xFFFFFF;
    uint32_t hint = 0xA0A0A0;

    gfx_fill_rect(r.x, r.y, r.w, r.h, bg);
    gfx_draw_rect(r.x, r.y, r.w, r.h, bd);

    int tx = r.x + 6;
    int ty = r.y + 6;

    if (text && text[0]) gfx_draw_text_utf8(tx, ty, fg, text);
    else gfx_draw_text_utf8(tx, ty, hint, "/");

    if (edit_mode) {
        int len = (int)strlen(text ? text : "");
        int cx = tx + len * 8;
        if (cx > r.x + r.w - 10) cx = r.x + r.w - 10;
        gfx_draw_text_utf8(cx, ty, 0xFF6A00, "_");
    }
}

static void draw_toolbar_ui(fileman_v2_t* f) {
    rect_t rt = r_toolbar(f->cw);
    gfx_fill_rect(rt.x, rt.y, rt.w, rt.h, 0x181818);
    gfx_draw_rect(rt.x, rt.y, rt.w, rt.h, 0x303030);

    draw_button(r_btn_up(), "^");
    draw_button(r_btn_refresh(), "R");

    const char* show = f->path_edit_mode ? f->path_buf : f->path;
    draw_pathbar(r_pathbar(f->cw), show, (f->path_edit_mode != 0));

    gfx_draw_text_utf8(f->cw - 110, 40, 0x808080, f->status);
}

// ------------------------------------------------------------
// content draw + hit
// ------------------------------------------------------------
static int fm_hit_item(fileman_v2_t* f, int mx, int my) {
    if (!f) return -1;

    rect_t rc = r_content(f->cw, f->ch);
    if (!pt_in(rc, mx, my)) return -1;

    int lx = mx - rc.x;
    int ly = my - rc.y + f->scroll_y;

    int cols = rc.w / CELL_W;
    if (cols < 1) cols = 1;

    int col = lx / CELL_W;
    int row = ly / CELL_H;

    int idx = row * cols + col;
    if (idx < 0 || idx >= f->item_count) return -1;

    return idx;
}

static void draw_item_cell(int x, int y, const fm2_item_t* it, bool selected) {
    if (selected) {
        gfx_fill_rect(x + 6, y + 6, CELL_W - 12, CELL_H - 12, 0x2A2A2A);
        gfx_draw_rect(x + 6, y + 6, CELL_W - 12, CELL_H - 12, 0xFF6A00);
    }

    int ix = x + (CELL_W - ICON20_W) / 2;
    int iy = y + 10;

    if (it->is_dir) {
        draw_icon20_scaled(ix, iy, folder_icon,
                           0xFF8A4A00,
                           0xFFFFB24A,
                           0xFF6A3200,
                           ICON20_SCALE);
    } else {
        draw_simple_file_icon20_scaled(ix, iy, ICON20_SCALE);
    }

    char display_name[FM2_NAME_MAX];

    const int max_chars = 12;
    int len = (int)strlen(it->name);

    if (len > max_chars) {
        int keep = max_chars - 3;
        if (keep < 1) keep = 1;

        strncpy(display_name, it->name, (size_t)keep);
        display_name[keep] = '\0';
        strncat(display_name, "...", sizeof(display_name) - strlen(display_name) - 1);
    } else {
        strncpy(display_name, it->name, sizeof(display_name) - 1);
        display_name[sizeof(display_name) - 1] = '\0';
    }

    gfx_draw_text_utf8(x + 8, y + 70, 0xFFFFFF, display_name);
}

static void draw_content(fileman_v2_t* f) {
    rect_t rc = r_content(f->cw, f->ch);
    gfx_fill_rect(rc.x, rc.y, rc.w, rc.h, 0x0B0B0B);

    int cols = rc.w / CELL_W;
    if (cols < 1) cols = 1;

    int start_x = rc.x + 8;
    int start_y = rc.y + 8 - f->scroll_y;

    for (int i = 0; i < f->item_count; i++) {
        int row = i / cols;
        int col = i % cols;

        int x = start_x + col * CELL_W;
        int y = start_y + row * CELL_H;

        if (y > rc.y + rc.h) break;
        if (y + CELL_H < rc.y) continue;

        draw_item_cell(x, y, &f->items[i], (i == f->selected));
    }
}

// ------------------------------------------------------------
// open action
// ------------------------------------------------------------
static void fm_open_selected(app_t* app) {
    fileman_v2_t* f = fm(app);
    if (!f) return;
    if (f->selected < 0 || f->selected >= f->item_count) return;

    fm2_item_t* it = &f->items[f->selected];

    if (it->is_dir) {
        fm_load_dir(f, it->full);
        return;
    }

    if (ends_with(it->name, ".html") || ends_with(it->name, ".htm")) {
        char url[FM2_PATH_MAX + 8];
        url[0] = 0;

        strncpy(url, "file:", sizeof(url) - 1);
        url[sizeof(url) - 1] = 0;
        strncat(url, it->full, sizeof(url) - strlen(url) - 1);

        app_t* bapp = appmgr_start_app(15);
        if (bapp) {
            kuvix_browser_open_url(bapp, url);
            fm_set_status(f, "Opened in browser");
        } else {
            fm_set_status(f, "Browser start failed");
        }
        return;
    }

    fm_set_status(f, "Open file: no handler");
}

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void fileman_on_create(app_t* app) {
    fileman_v2_t* f = fm(app);
    if (!f) return;
    memset(f, 0, sizeof(*f));

    f->selected = -1;
    f->last_click_index = -1;
    f->last_click_tick_ms = 0;
    f->path_edit_mode = 0;

    // ✅ default: user's desktop parent ("/home/anil" gibi)
    char desktop[FM2_PATH_MAX];
    char start[FM2_PATH_MAX];

    user_get_desktop_path(desktop, (int)sizeof(desktop));
    fm_parent_dir(desktop, start, (int)sizeof(start));
    if (!start[0]) strncpy(start, "/", sizeof(start) - 1);

    fm_load_dir(f, start);
}

static void fileman_on_draw(app_t* app) {
    fileman_v2_t* f = fm(app);
    if (!f) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    f->cw = client.w;
    f->ch = client.h;

    gfx_fill_rect(0, 0, client.w, client.h, 0x000000);

    draw_toolbar_ui(f);
    draw_content(f);
}

static void fileman_on_mouse(app_t* app, int mx, int my,
                             uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)released; (void)buttons;
    fileman_v2_t* f = fm(app);
    if (!f) return;
    if (!(pressed & 0x01)) return;

    if (pt_in(r_btn_up(), mx, my)) {
        char parent[FM2_PATH_MAX];
        fm_parent_dir(f->path, parent, (int)sizeof(parent));
        fm_load_dir(f, parent);
        return;
    }

    if (pt_in(r_btn_refresh(), mx, my)) {
        fm_load_dir(f, f->path);
        fm_set_status(f, "Refreshed");
        return;
    }

    if (pt_in(r_pathbar(f->cw), mx, my)) {
        f->path_edit_mode = 1;

        strncpy(f->path_buf, f->path, sizeof(f->path_buf) - 1);
        f->path_buf[sizeof(f->path_buf) - 1] = 0;
        f->path_len = (int)strlen(f->path_buf);

        fm_set_status(f, "Editing path (Enter=open, Esc=cancel)");
        return;
    }

    if (f->path_edit_mode) {
        f->path_edit_mode = 0;
        fm_set_status(f, "Ready");
    }

    int idx = fm_hit_item(f, mx, my);
    if (idx < 0) return;

    f->selected = idx;

    uint32_t now = fm_ticks_ms();
    if (f->last_click_index == idx) {
        uint32_t dt = now - f->last_click_tick_ms;
        if (dt <= 350) {
            fm_open_selected(app);
            f->last_click_index = -1;
            f->last_click_tick_ms = 0;
            return;
        }
    }
    f->last_click_index = idx;
    f->last_click_tick_ms = now;
}

static void fileman_on_key(app_t* app, uint16_t key) {
    fileman_v2_t* f = fm(app);
    if (!f) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return;

    if (f->path_edit_mode) {
        if (sc == 0x1C) {
            f->path_edit_mode = 0;
            fm_load_dir(f, f->path_buf[0] ? f->path_buf : "/");
            return;
        }

        if (sc == 0x01) {
            f->path_edit_mode = 0;
            fm_set_status(f, "Ready");
            return;
        }

        if (sc == 0x0E) {
            if (f->path_len > 0) {
                f->path_len--;
                f->path_buf[f->path_len] = '\0';
            }
            return;
        }

        char c = kbd_scancode_to_ascii(sc);
        if (c >= 32 && c <= 126) {
            if (f->path_len < FM2_PATH_MAX - 1) {
                f->path_buf[f->path_len++] = c;
                f->path_buf[f->path_len] = '\0';
            }
            return;
        }

        return;
    }

    if (sc == 0x1C) { fm_open_selected(app); return; }
    if (sc == 0x3F) { fm_load_dir(f, f->path); fm_set_status(f, "Refreshed"); return; }

    if (sc == 0x0E) {
        char parent[FM2_PATH_MAX];
        fm_parent_dir(f->path, parent, (int)sizeof(parent));
        fm_load_dir(f, parent);
        return;
    }
}

static void fileman_on_destroy(app_t* app) { (void)app; }

const app_vtbl_t fileman_v2_vtbl = {
    .on_create  = fileman_on_create,
    .on_draw    = fileman_on_draw,
    .on_mouse   = fileman_on_mouse,
    .on_key     = fileman_on_key,
    .on_destroy = fileman_on_destroy
};