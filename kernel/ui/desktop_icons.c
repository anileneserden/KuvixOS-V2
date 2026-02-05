#include <ui/desktop_icons.h>
#include <ui/desktop_icons/text_file.h>
#include <ui/desktop_icons/generic_file.h>
#include <ui/desktop_icons/folder_icon.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <app/app_manager.h>
#include <stdbool.h>

// --- DIŞ BİLDİRİMLER ---
extern void notepad_open_file(const char* vfs_path);
extern int mouse_x;
extern int mouse_y;
extern void desktop_handle_rename_confirm(const char* new_name);

#define MAX_DESKTOP_ICONS 32
static desktop_icon_t icons[MAX_DESKTOP_ICONS];
static int icon_count = 0;
static bool snap_to_grid = true;

// --- YARDIMCI FONKSİYONLAR ---

static bool ends_with(const char* str, const char* suffix) {
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static int desktop_load_callback(const char* path, uint32_t size, void* u) {
    (void)size; (void)u;
    if (icon_count >= MAX_DESKTOP_ICONS) return 0;

    if (strcmp(path, "/home") == 0 || strcmp(path, "/home/desktop") == 0 || 
        strcmp(path, "/home/desktop/") == 0 || strcmp(path, "/") == 0) {
        return 1; 
    }

    const char* filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;

    if (filename[0] == '\0' || filename[0] == '.') return 1;

    strncpy(icons[icon_count].vfs_name, path, 63); 
    strncpy(icons[icon_count].label, filename, 31); 

    vfs_stat_t st;
    bool stat_ok = (vfs_stat(path, &st) == 0);
    bool is_txt_ext = ends_with(filename, ".txt");

    if (is_txt_ext) {
        icons[icon_count].is_dir = false;
        icons[icon_count].app_id = 4;
    } 
    else if (stat_ok) {
        icons[icon_count].is_dir = (st.type == VFS_T_DIR);
        icons[icon_count].app_id = 0;
    }
    else if (strchr(filename, '.') != NULL) {
        icons[icon_count].is_dir = false;
        icons[icon_count].app_id = 0;
    }
    else {
        icons[icon_count].is_dir = true;
        icons[icon_count].app_id = 0;
    }

    icons[icon_count].x = 40 + (icon_count / 5 * 100);
    icons[icon_count].y = 40 + (icon_count % 5 * 90);
    
    icons[icon_count].is_selected = false;
    icons[icon_count].dragging = false;
    icons[icon_count].is_editing = false; // Başlangıçta kapalı

    icon_count++;
    return 1;
}

void desktop_icons_init(void) {
    icon_count = 0;
    memset(icons, 0, sizeof(icons));

    strncpy(icons[icon_count].label, "Notepad", 31);
    icons[icon_count].x = 40;
    icons[icon_count].y = 40;
    icons[icon_count].app_id = 4;
    icons[icon_count].is_dir = false;
    icon_count++;

    vfs_list("/home/desktop", desktop_load_callback, 0);
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
        bool actually_draw_dir = icon->is_dir && !is_txt;

        for (int r = 0; r < 20; r++) {
            for (int c = 0; c < 20; c++) {
                uint8_t p = 0;
                if (actually_draw_dir) p = folder_icon[r][c];
                else if (icon->app_id == 4 || is_txt) p = text_file_icon[r][c];
                else p = generic_file_icon[r][c];

                uint32_t color = 0;
                if (p == 1)      color = 0x000000;
                else if (p == 2) color = (actually_draw_dir) ? 0xFFCC00 : 0xFFFFFF;
                else if (p == 3) color = 0xCC9900;
                else if (p == 4) color = 0xFFFFFF;
                
                if (p != 0) fb_putpixel(icon->x + c + 6, icon->y + r + 6, color);
            }
        }

        // --- YENİ MANTIK: DÜZENLEME MODU ÇİZİMİ ---
        if (icon->is_editing) {
            // Mavi Arka Plan (Seçili Metin Efekti)
            int text_w = strlen(icon->edit_buffer) * 8 + 4;
            gfx_fill_rect(icon->x - 4, icon->y + 30, text_w, 12, 0x0078D7);
            gfx_draw_text(icon->x - 2, icon->y + 32, 0xFFFFFF, icon->edit_buffer);
        } else {
            uint32_t text_color = (icon->is_selected || is_hover) ? 0xFFFFFF : 0xEEEEEE;
            gfx_draw_text(icon->x - 4, icon->y + 30, text_color, icon->label);
        }
    }
}

// Klavye Girişini İşle
void desktop_icons_handle_key(uint16_t scancode, char ascii) {
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].is_editing) {
            int len = strlen(icons[i].edit_buffer);
            if (ascii == '\n' || ascii == '\r') {
                icons[i].is_editing = false;
                desktop_handle_rename_confirm(icons[i].edit_buffer);
            } else if (ascii == 8) { // Backspace
                if (len > 0) icons[i].edit_buffer[len-1] = '\0';
            } else if (ascii >= 32 && len < 31) {
                icons[i].edit_buffer[len] = ascii;
                icons[i].edit_buffer[len+1] = '\0';
            }
            return;
        }
    }
}

// Düzenleme modunu başlat
void desktop_icons_begin_edit(int index) {
    if (index < 0 || index >= icon_count) return;
    for (int i = 0; i < icon_count; i++) icons[i].is_editing = false; // Diğerlerini kapat
    icons[index].is_editing = true;
    strcpy(icons[index].edit_buffer, icons[index].label);
}

bool desktop_icons_is_any_editing(void) {
    for (int i = 0; i < icon_count; i++) if (icons[i].is_editing) return true;
    return false;
}

// Diğer fonksiyonlar (get_hit, move, delete vs.) olduğu gibi kalıyor...
int desktop_icons_get_hit(int mx, int my) {
    for (int i = 0; i < icon_count; i++) {
        if (mx >= icons[i].x && mx <= icons[i].x + 32 &&
            my >= icons[i].y && my <= icons[i].y + 32) return i;
    }
    return -1;
}

void desktop_icons_process_click(int index) {
    if (index < 0 || index >= icon_count) return;
    desktop_icon_t* icon = &icons[index];

    // Eğer bir klasörse şimdilik işlem yapma
    if (icon->is_dir) return;

    // Eğer .txt dosyasıysa veya Notepad ID'sine sahipse Notepad ile aç
    if (strstr(icon->label, ".txt") != NULL || icon->app_id == 4) {
        notepad_open_file(icon->vfs_name);
    }
    
    // Uygulama ID'si varsa başlat
    if (icon->app_id > 0) {
        appmgr_start_app(icon->app_id);
    }
}

void desktop_icons_deselect_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].is_selected = false;
        icons[i].is_editing = false; // Seçim değişince düzenlemeyi kapat
    }
}

void desktop_icons_reset_selection(void) { desktop_icons_deselect_all(); }
void desktop_icons_select(int index) { if (index >= 0 && index < icon_count) icons[index].is_selected = true; }
void desktop_icons_move_dragging(int mx, int my) {
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].dragging) { icons[i].x = mx - 16; icons[i].y = my - 16; }
    }
}
void desktop_icons_set_dragging(int index, bool state) {
    if (index >= 0 && index < icon_count) { icons[index].dragging = state; icons[index].is_selected = state; }
}
void desktop_icons_stop_dragging_all(void) { for (int i = 0; i < icon_count; i++) icons[i].dragging = false; }
void desktop_icons_snap_all(void) {
    if (!snap_to_grid) return;
    for (int i = 0; i < icon_count; i++) {
        icons[i].x = (icons[i].x / 80) * 80 + 10;
        icons[i].y = (icons[i].y / 80) * 80 + 10;
        if (icons[i].y < 35) icons[i].y = 40; 
    }
}
void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2) {
    int min_x = (x1 < x2) ? x1 : x2; int max_x = (x1 > x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2; int max_y = (y1 > y2) ? y1 : y2;
    for (int i = 0; i < icon_count; i++) {
        bool overlap = !(icons[i].x + 32 < min_x || icons[i].x > max_x || icons[i].y + 32 < min_y || icons[i].y > max_y);
        if (overlap) icons[i].is_selected = true;
    }
}
void desktop_icons_delete_selected(void) {
    for (int i = icon_count - 1; i >= 0; i--) {
        if (icons[i].is_selected) {
            vfs_remove(icons[i].vfs_name);
            for (int j = i; j < icon_count - 1; j++) icons[j] = icons[j + 1];
            icon_count--;
        }
    }
}
int desktop_icons_get_count(void) { return icon_count; }
const char* desktop_icons_get_name(int index) {
    if (index >= 0 && index < icon_count) return icons[index].label;
    return "";
}