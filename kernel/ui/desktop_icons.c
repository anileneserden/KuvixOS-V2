// kernel/ui/desktop_icons.c

#include <ui/desktop_icons.h>
#include <ui/desktop.h>
#include <ui/desktop_icons/text_file.h>
#include <ui/desktop_icons/generic_file.h>
#include <ui/desktop_icons/folder_icon.h>
#include <ui/desktop.h>

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

    // full path
    strncpy(ic->vfs_name, full_path, 63);
    ic->vfs_name[63] = '\0';

    // label
    strncpy(ic->label, label, 31);
    ic->label[31] = '\0';

    ic->is_dir = is_dir;

    // basit dizilim
    ic->x = 40 + (icon_count / 5 * 100);
    ic->y = 40 + (icon_count % 5 * 90);

    ic->is_selected = false;
    ic->dragging = false;
    ic->is_editing = false;
    ic->edit_buffer[0] = '\0';

    icon_count++;
}

// ------------------------------------------------------------
// vfs_list callback -> ikon üret
// ------------------------------------------------------------
static int desktop_load_callback(const char* path, uint32_t size, void* u) {
    (void)size; (void)u;

    if (icon_count >= MAX_DESKTOP_ICONS) return 0;

    // Bazı root entry'leri atla (güvenlik)
    if (!path || path[0] == '\0') return 1;
    if (strcmp(path, "/home") == 0 ||
        strcmp(path, "/home/desktop") == 0 ||
        strcmp(path, "/home/desktop/") == 0 ||
        strcmp(path, "/") == 0) {
        return 1;
    }

    const char* filename = base_name(path);

    // gizli dosyalar / boş isim atla
    if (filename[0] == '\0' || filename[0] == '.') return 1;

    // dir mi?
    vfs_stat_t st;
    bool stat_ok = (vfs_stat(path, &st) == 1);
    bool is_dir = false;

    if (stat_ok) is_dir = (st.type == VFS_T_DIR);

    // klasör
    if (is_dir) {
        icons_add(path, filename, true);
        return 1;
    }

    // .ksf -> kısayol dosyası (parse AppManager tarafında)
    if (ends_with(filename, ".ksf")) {
        icons_add(path, filename, false);
        return 1;
    }

    // .txt -> text dosyası
    if (ends_with(filename, ".txt")) {
        icons_add(path, filename, false);
        return 1;
    }

    // diğer dosyalar
    icons_add(path, filename, false);
    return 1;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void desktop_icons_init(void) {
    icons_clear();

    // Masaüstünü tara
    vfs_list(USER_DESKTOP_PATH, desktop_load_callback, 0);

    desktop_icons_snap_all();
}

void desktop_icons_draw_all(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int i = 0; i < icon_count; i++) {
        desktop_icon_t* icon = &icons[i];

        bool is_hover = (mx >= icon->x && mx <= icon->x + 32 &&
                         my >= icon->y && my <= icon->y + 32);

        if (icon->is_selected) {
            gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 50, 0x0055AA);
        } else if (is_hover) {
            gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 50, 0x333333);
        }

        bool is_txt = ends_with(icon->label, ".txt");
        bool is_ksf = ends_with(icon->label, ".ksf");
        bool actually_draw_dir = icon->is_dir;

        // ikon çizimi
        for (int r = 0; r < 20; r++) {
            for (int c = 0; c < 20; c++) {
                uint8_t p = 0;
                if (actually_draw_dir) p = folder_icon[r][c];
                else if (is_txt || is_ksf) p = text_file_icon[r][c]; // şimdilik ksf'yi text ikonla
                else p = generic_file_icon[r][c];

                uint32_t color = 0;
                if (p == 1)      color = 0x000000;
                else if (p == 2) color = (actually_draw_dir) ? 0xFFCC00 : 0xFFFFFF;
                else if (p == 3) color = 0xCC9900;
                else if (p == 4) color = 0xFFFFFF;

                if (p != 0) fb_putpixel(icon->x + c + 6, icon->y + r + 6, color);
            }
        }

        // düzenleme mod çizimi
        if (icon->is_editing) {
            int text_w = (int)strlen(icon->edit_buffer) * 8 + 4;
            gfx_fill_rect(icon->x - 4, icon->y + 30, text_w, 12, 0x0078D7);
            gfx_draw_text(icon->x - 2, icon->y + 32, 0xFFFFFF, icon->edit_buffer);
        } else {
            uint32_t text_color = (icon->is_selected || is_hover) ? 0xFFFFFF : 0xEEEEEE;
            gfx_draw_text(icon->x - 4, icon->y + 34, text_color, icon->label);
        }
    }
}

// Klavye girişini işle (rename edit buffer)
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
        // ikon 32x32 + label alanı (~50px)
        int w = 40;
        int h = 52;
        int x = icons[i].x - 4;
        int y = icons[i].y - 4;

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

    // açma mantığı AppManager'da
    appmgr_open_path(icon->vfs_name);
}

void desktop_icons_deselect_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].is_selected = false;
        icons[i].is_editing = false;
    }
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
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].dragging) {
            icons[i].x = mx - 16;
            icons[i].y = my - 16;
        }
    }
}

void desktop_icons_set_dragging(int index, bool state) {
    if (index >= 0 && index < icon_count) {
        icons[index].dragging = state;
        icons[index].is_selected = state;
    }
}

void desktop_icons_stop_dragging_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].dragging = false;
    }
}

void desktop_icons_snap_all(void) {
    if (!snap_to_grid) return;
    for (int i = 0; i < icon_count; i++) {
        icons[i].x = (icons[i].x / 80) * 80 + 10;
        icons[i].y = (icons[i].y / 80) * 80 + 10;
        if (icons[i].y < 35) icons[i].y = 40;
    }
}

void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2) {
    int min_x = (x1 < x2) ? x1 : x2;
    int max_x = (x1 > x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_y = (y1 > y2) ? y1 : y2;

    for (int i = 0; i < icon_count; i++) {
        bool overlap = !(icons[i].x + 32 < min_x || icons[i].x > max_x ||
                         icons[i].y + 32 < min_y || icons[i].y > max_y);
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