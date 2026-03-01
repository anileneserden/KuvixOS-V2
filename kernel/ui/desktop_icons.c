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
static int drag_off_x[MAX_DESKTOP_ICONS];
static int drag_off_y[MAX_DESKTOP_ICONS];

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

static void strip_ext(char* out, int out_sz, const char* name, const char* ext) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';
    if (!name) return;

    // default: name'i kopyala
    strncpy(out, name, out_sz - 1);
    out[out_sz - 1] = '\0';

    if (!ext) return;

    int nlen = (int)strlen(out);
    int elen = (int)strlen(ext);

    if (nlen >= elen && strcmp(out + (nlen - elen), ext) == 0) {
        out[nlen - elen] = '\0'; // ext'i kırp
    }
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
        strcmp(path, USER_DESKTOP_PATH) == 0 ||
        strcmp(path, USER_DESKTOP_PATH "/") == 0 ||
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
        char label[32];
        strip_ext(label, sizeof(label), filename, ".ksf");
        // boş kalırsa fallback
        if (label[0] == '\0') strncpy(label, filename, sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';

        icons_add(path, label, false);   // ✅ uzantısız label
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

// 20x20 ikon bitmap'ini 2x scale ile kart içine çizer
static void draw_icon_card72(
    int x, int y,
    const char* label,
    bool hover,
    bool selected,
    const uint8_t icon20[20][20],
    bool is_dir
) {
    const int w = 72, h = 72, rad = 12;

    uint32_t col_shadow = 0x101010;

    // normal gri
    uint32_t col_norm   = 0x2A2A2A;

    // hover / selected mavi
    uint32_t col_hover  = 0x0078D7;
    uint32_t col_sel    = 0x005FB3;

    uint32_t card = col_norm;
    if (selected) card = col_sel;
    else if (hover) card = col_hover;

    // shadow + kart
    gfx_fill_round_rect(x + 2, y + 2, w, h, rad, col_shadow);
    gfx_fill_round_rect(x, y, w, h, rad, card);

    // --- ikon (2x scale) ---
    const int scale = 2;
    const int base = 20;
    const int icon_w = base * scale;
    const int pad_top = 8;

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
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        gfx_putpixel(ix + cc*scale + sx, iy + rr*scale + sy, col);
            }
        }
    }

    // --- label (kart içinde altta, ortalı) ---
    if (!label) label = "";

    // çok uzunsa kısalt (kartta taşmasın)
    char tmp[32];
    int len = (int)strlen(label);
    if (len > 10) {
        // 8 char + ".."
        int k = 0;
        for (; k < 8 && k < (int)sizeof(tmp)-1; k++) tmp[k] = label[k];
        if (k < (int)sizeof(tmp)-3) { tmp[k++]='.'; tmp[k++]='.'; }
        tmp[k] = 0;
        label = tmp;
        len = (int)strlen(label);
    }

    int label_w = len * 8;
    int label_h = 16;
    int pad_bottom = 6;

    int text_x = x + (w - label_w) / 2;
    int text_y = y + h - label_h - pad_bottom;

    gfx_draw_text_utf8(text_x, text_y, 0x00FFFFFF, label);
}

void desktop_icons_draw_all(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int i = 0; i < icon_count; i++) {
        desktop_icon_t* icon = &icons[i];

        // Kart hitbox: 72x72
        bool is_hover = (mx >= icon->x && mx <= icon->x + 72 &&
                         my >= icon->y && my <= icon->y + 72);

        // hangi ikon bitmap?
        const uint8_t (*bmp)[20] = generic_file_icon;

        // NOTE: icon->label içinde .ksf uzantısını kırpmıştın, o yüzden is_ksf label'dan çıkmayabilir.
        // Bu yüzden path'e de bakmak mantıklı:
        bool is_txt  = ends_with(icon->vfs_name, ".txt");
        bool is_ksf  = ends_with(icon->vfs_name, ".ksf");
        bool is_html = ends_with(icon->vfs_name, ".html");

        if (icon->is_dir) bmp = folder_icon;
        else if (is_txt || is_ksf) bmp = text_file_icon;
        else bmp = generic_file_icon;

        draw_icon_card72(
            icon->x, icon->y,
            icon->label,
            is_hover,
            icon->is_selected,
            bmp,
            icon->is_dir
        );

        // rename edit modunu şimdilik sonra entegre edelim
        // (istersen bunu da karta entegre edebiliriz)
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
        int w = 72;
        int h = 72;

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
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].dragging) {
            icons[i].x = mx - drag_off_x[i];
            icons[i].y = my - drag_off_y[i];
        }
    }
}

void desktop_icons_set_dragging(int index, bool state, int mx, int my) {
    if (!state) {
        // drag kapat
        for (int i = 0; i < icon_count; i++) icons[i].dragging = false;
        return;
    }

    if (index < 0 || index >= icon_count) return;

    // Eğer tıklanan ikon seçili değilse -> sadece onu seç (grup değil tek drag)
    if (!icons[index].is_selected) {
        desktop_icons_deselect_all();
        icons[index].is_selected = true;
    }

    // Seçili ikonların hepsini drag moduna al + offset kaydet
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

    const int cell = 90;      // aralık
    const int offx = 20;
    const int offy = 40;

    for (int i = 0; i < icon_count; i++) {
        icons[i].x = (icons[i].x / cell) * cell + offx;
        icons[i].y = (icons[i].y / cell) * cell + offy;
        if (icons[i].y < 35) icons[i].y = 40;
    }
}

void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2) {
    int min_x = (x1 < x2) ? x1 : x2;
    int max_x = (x1 > x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_y = (y1 > y2) ? y1 : y2;

    const int W = 72;
    const int H = 72;

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