#include <ui/desktop_icons.h>
#include <ui/desktop.h>
#include <ui/desktop_icons/text_file.h>
#include <ui/desktop_icons/generic_file.h>
#include <ui/desktop_icons/folder_icon.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <app/app_manager.h>
#include <stdbool.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/printk.h>
#include <kernel/user.h>

// --- DIŞ BİLDİRİMLER ---
extern int mouse_x;
extern int mouse_y;

#define MAX_DESKTOP_ICONS 32

static desktop_icon_t icons[MAX_DESKTOP_ICONS];
static int icon_count = 0;
static bool snap_to_grid = true;
static int drag_off_x[MAX_DESKTOP_ICONS];
static int drag_off_y[MAX_DESKTOP_ICONS];

static desktop_icons_style_t g_style = {
    .card_width = 72,
    .card_height = 72,
    .corner_radius = 12,

    .grid_cell = 90,
    .offset_x = 20,
    .offset_y = 40,

    .icon_base = 20,
    .icon_scale = 2,
    .icon_pad_top = 8,

    .label_pad_bottom = 6,
    .label_max_chars = 10,

    .shadow_color = 0x101010,
    .normal_color = 0x2A2A2A,
    .hover_color = 0x0078D7,
    .selected_color = 0x005FB3,
    .text_color = 0x00FFFFFF,
};

void desktop_icons_set_style(const desktop_icons_style_t* style) {
    if (!style) return;
    g_style = *style;
}

const desktop_icons_style_t* desktop_icons_get_style(void) {
    return &g_style;
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static bool ends_with(const char* str, const char* suffix) {
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static const char* base_name(const char* path) {
    const char* p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static void icons_clear(void) {
    icon_count = 0;
    memset(icons, 0, sizeof(icons));
}

// icon ekle (internal)
static void icons_add(const char* full_path, const char* label, bool is_dir) {
    if (!full_path || !label) return;
    if (icon_count >= MAX_DESKTOP_ICONS) return;

    desktop_icon_t* ic = &icons[icon_count];
    memset(ic, 0, sizeof(*ic));

    strncpy(ic->vfs_name, full_path, 63);
    ic->vfs_name[63] = '\0';

    strncpy(ic->label, label, 31);
    ic->label[31] = '\0';

    ic->is_dir = is_dir;

    // style ile uyumlu kaba başlangıç yerleşimi
    {
        int col = icon_count / 5;
        int row = icon_count % 5;
        ic->x = g_style.offset_x + (col * g_style.grid_cell);
        ic->y = g_style.offset_y + (row * g_style.grid_cell);
    }

    ic->is_selected = false;
    ic->dragging = false;
    ic->is_editing = false;
    ic->edit_buffer[0] = '\0';

    icon_count++;
}

static void strip_ext(char* out, int out_sz, const char* name, const char* ext) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';
    if (!name) return;

    strncpy(out, name, out_sz - 1);
    out[out_sz - 1] = '\0';

    if (!ext) return;

    int nlen = (int)strlen(out);
    int elen = (int)strlen(ext);

    if (nlen >= elen && strcmp(out + (nlen - elen), ext) == 0) {
        out[nlen - elen] = '\0';
    }
}

// ------------------------------------------------------------
// vfs_list callback -> ikon üret
// ------------------------------------------------------------
static int desktop_load_callback(const char* path, uint32_t size, void* u) {
    (void)size;
    (void)u;

    if (icon_count >= MAX_DESKTOP_ICONS) return 0;

    if (!path || path[0] == '\0') return 1;
    if (strcmp(path, "/home") == 0 ||
        strcmp(path, "/home/desktop") == 0 ||
        strcmp(path, "/home/desktop/") == 0 ||
        strcmp(path, USER_DESKTOP_PATH) == 0 ||
        strcmp(path, USER_DESKTOP_PATH "/") == 0 ||
        strcmp(path, "/") == 0) {
        return 1;
    }

    const char* filename = base_name(path);

    if (filename[0] == '\0' || filename[0] == '.') return 1;

    vfs_stat_t st;
    bool stat_ok = (vfs_stat(path, &st) == 1);
    bool is_dir = false;

    if (stat_ok) is_dir = (st.type == VFS_T_DIR);

    if (is_dir) {
        icons_add(path, filename, true);
        return 1;
    }

    if (ends_with(filename, ".ksf")) {
        char label[32];
        strip_ext(label, sizeof(label), filename, ".ksf");
        if (label[0] == '\0') strncpy(label, filename, sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';

        icons_add(path, label, false);
        return 1;
    }

    if (ends_with(filename, ".txt")) {
        icons_add(path, filename, false);
        return 1;
    }

    icons_add(path, filename, false);
    return 1;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void desktop_icons_init(void) {
    icons_clear();
    vfs_list(USER_DESKTOP_PATH, desktop_load_callback, 0);
    desktop_icons_snap_all();
}

static void draw_icon_card(
    int x, int y,
    const char* label,
    bool hover,
    bool selected,
    const uint8_t icon20[20][20],
    bool is_dir
) {
    const int w = g_style.card_width;
    const int h = g_style.card_height;
    const int rad = g_style.corner_radius;

    uint32_t card = g_style.normal_color;
    if (selected) card = g_style.selected_color;
    else if (hover) card = g_style.hover_color;

    gfx_fill_round_rect(x + 2, y + 2, w, h, rad, g_style.shadow_color);
    gfx_fill_round_rect(x, y, w, h, rad, card);

    // ikon
    {
        const int scale = g_style.icon_scale;
        const int base = g_style.icon_base;
        const int icon_w = base * scale;
        const int pad_top = g_style.icon_pad_top;

        int ix = x + (w - icon_w) / 2;
        int iy = y + pad_top;

        for (int rr = 0; rr < base; rr++) {
            for (int cc = 0; cc < base; cc++) {
                uint8_t p = icon20[rr][cc];
                uint32_t col = 0;

                if (p == 1)      col = 0x000000;
                else if (p == 2) col = is_dir ? 0xFFCC00 : 0xFFFFFF;
                else if (p == 3) col = is_dir ? 0xCC9900 : 0xE6E6E6;
                else if (p == 4) col = 0xFFFFFF;

                if (p != 0) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            gfx_putpixel(ix + cc * scale + sx,
                                         iy + rr * scale + sy,
                                         col);
                        }
                    }
                }
            }
        }
    }

    // label
    if (!label) label = "";

    {
        char tmp[32];
        int len = (int)strlen(label);

        if (len > g_style.label_max_chars) {
            int keep = g_style.label_max_chars - 2;
            int k = 0;
            if (keep < 1) keep = 1;

            for (; k < keep && k < (int)sizeof(tmp) - 1; k++) {
                tmp[k] = label[k];
            }
            if (k < (int)sizeof(tmp) - 3) {
                tmp[k++] = '.';
                tmp[k++] = '.';
            }
            tmp[k] = 0;
            label = tmp;
            len = (int)strlen(label);
        }

        {
            int label_w = len * 8;
            int label_h = 16;
            int pad_bottom = g_style.label_pad_bottom;

            int text_x = x + (w - label_w) / 2;
            int text_y = y + h - label_h - pad_bottom;

            gfx_draw_text_utf8(text_x, text_y, g_style.text_color, label);
        }
    }
}

void desktop_icons_draw_all(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int i = 0; i < icon_count; i++) {
        desktop_icon_t* icon = &icons[i];

        bool is_hover =
            (mx >= icon->x && mx <= icon->x + g_style.card_width &&
             my >= icon->y && my <= icon->y + g_style.card_height);

        const uint8_t (*bmp)[20] = generic_file_icon;

        bool is_txt  = ends_with(icon->vfs_name, ".txt");
        bool is_ksf  = ends_with(icon->vfs_name, ".ksf");
        bool is_html = ends_with(icon->vfs_name, ".html");
        (void)is_html;

        if (icon->is_dir) bmp = folder_icon;
        else if (is_txt || is_ksf) bmp = text_file_icon;
        else bmp = generic_file_icon;

        draw_icon_card(
            icon->x, icon->y,
            icon->label,
            is_hover,
            icon->is_selected,
            bmp,
            icon->is_dir
        );
    }
}

void desktop_icons_handle_key(uint16_t scancode, char ascii) {
    (void)scancode;

    for (int i = 0; i < icon_count; i++) {
        if (icons[i].is_editing) {
            int len = (int)strlen(icons[i].edit_buffer);

            if (ascii == '\n' || ascii == '\r') {
                icons[i].is_editing = false;
                desktop_handle_rename_confirm(icons[i].edit_buffer);
            } else if (ascii == 8) {
                if (len > 0) icons[i].edit_buffer[len - 1] = '\0';
            } else if (ascii >= 32 && len < 31) {
                icons[i].edit_buffer[len] = ascii;
                icons[i].edit_buffer[len + 1] = '\0';
            }
            return;
        }
    }
}

const char* desktop_icons_get_path(int index) {
    if (index >= 0 && index < icon_count)
        return icons[index].vfs_name;
    return "";
}

void desktop_icons_begin_edit(int index) {
    if (index < 0 || index >= icon_count) return;
    for (int i = 0; i < icon_count; i++) icons[i].is_editing = false;
    icons[index].is_editing = true;
    strcpy(icons[index].edit_buffer, icons[index].label);
}

bool desktop_icons_is_any_editing(void) {
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].is_editing) return true;
    }
    return false;
}

int desktop_icons_get_hit(int mx, int my) {
    for (int i = 0; i < icon_count; i++) {
        int x = icons[i].x;
        int y = icons[i].y;
        int w = g_style.card_width;
        int h = g_style.card_height;

        if (mx >= x && mx <= x + w &&
            my >= y && my <= y + h) {
            return i;
        }
    }
    return -1;
}

void desktop_icons_process_click(int index) {
    if (index < 0 || index >= icon_count) return;
    desktop_icon_t* icon = &icons[index];

    printk("[Desktop] open: label=%s path=%s\n", icon->label, icon->vfs_name);
    appmgr_open_path(icon->vfs_name);
}

void desktop_icons_deselect_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].is_selected = false;
        icons[i].is_editing = false;
    }
}

bool desktop_icons_is_selected(int index) {
    if (index < 0 || index >= icon_count) return false;
    return icons[index].is_selected;
}

void desktop_icons_toggle_select(int index) {
    if (index < 0 || index >= icon_count) return;
    icons[index].is_selected = !icons[index].is_selected;
}

void desktop_icons_reset_selection(void) {
    desktop_icons_deselect_all();
}

void desktop_icons_select(int index) {
    if (index >= 0 && index < icon_count) {
        icons[index].is_selected = true;
    }
}

void desktop_icons_move_dragging(int mx, int my) {
    const int W = g_style.card_width;
    const int H = g_style.card_height;
    const int PAD = 6;

    for (int i = 0; i < icon_count; i++) {
        if (icons[i].dragging) {
            int oldx = icons[i].x;
            int oldy = icons[i].y;

            int newx = mx - drag_off_x[i];
            int newy = my - drag_off_y[i];

            if (newx == oldx && newy == oldy) continue;

            desktop_damage_rect(oldx - PAD, oldy - PAD, W + PAD * 2 + 2, H + PAD * 2 + 2);

            icons[i].x = newx;
            icons[i].y = newy;

            desktop_damage_rect(newx - PAD, newy - PAD, W + PAD * 2 + 2, H + PAD * 2 + 2);
        }
    }

    desktop_request_redraw();
}

void desktop_icons_set_dragging(int index, bool state, int mx, int my) {
    if (!state) {
        for (int i = 0; i < icon_count; i++) icons[i].dragging = false;
        return;
    }

    if (index < 0 || index >= icon_count) return;

    if (!icons[index].is_selected) {
        desktop_icons_deselect_all();
        icons[index].is_selected = true;
    }

    for (int i = 0; i < icon_count; i++) {
        if (icons[i].is_selected) {
            icons[i].dragging = true;
            drag_off_x[i] = mx - icons[i].x;
            drag_off_y[i] = my - icons[i].y;
        } else {
            icons[i].dragging = false;
        }
    }
}

void desktop_icons_stop_dragging_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].dragging = false;
    }
}

void desktop_icons_snap_all(void) {
    if (!snap_to_grid) return;

    const int cell = g_style.grid_cell;
    const int offx = g_style.offset_x;
    const int offy = g_style.offset_y;

    for (int i = 0; i < icon_count; i++) {
        icons[i].x = (icons[i].x / cell) * cell + offx;
        icons[i].y = (icons[i].y / cell) * cell + offy;
        if (icons[i].y < 35) icons[i].y = offy;
    }
}

void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2) {
    int min_x = (x1 < x2) ? x1 : x2;
    int max_x = (x1 > x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_y = (y1 > y2) ? y1 : y2;

    const int W = g_style.card_width;
    const int H = g_style.card_height;

    for (int i = 0; i < icon_count; i++) {
        bool overlap = !(icons[i].x + W < min_x || icons[i].x > max_x ||
                         icons[i].y + H < min_y || icons[i].y > max_y);
        if (overlap) icons[i].is_selected = true;
    }
}

void desktop_icons_delete_selected(void) {
    for (int i = icon_count - 1; i >= 0; i--) {
        if (icons[i].is_selected) {
            vfs_remove(icons[i].vfs_name);
            for (int j = i; j < icon_count - 1; j++) {
                icons[j] = icons[j + 1];
            }
            icon_count--;
        }
    }
}

int desktop_icons_get_count(void) {
    return icon_count;
}

const char* desktop_icons_get_name(int index) {
    if (index >= 0 && index < icon_count) {
        return icons[index].label;
    }
    return "";
}

bool desktop_icons_get_rect(int index, int* x, int* y, int* w, int* h) {
    if (index < 0 || index >= icon_count) return false;
    if (!x || !y || !w || !h) return false;

    *x = icons[index].x;
    *y = icons[index].y;
    *w = g_style.card_width;
    *h = g_style.card_height;
    return true;
}