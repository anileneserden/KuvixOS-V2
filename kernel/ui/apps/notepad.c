#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <ui/dialogs/save_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/fs/vfs.h>
#include <ui/notification.h>
#include <ui/apps/notepad.h> // notepad_t buradan geliyor

// --- DIŞ BİLDİRİMLER ---
extern char kbd_scancode_to_ascii(uint8_t scancode);
extern int wm_get_mouse_x(void);
extern int wm_get_mouse_y(void);

// Callback'lerde erişebilmek için global pointer
static notepad_t* global_data = NULL;
// Menüye "Farklı Kaydet" eklendi
static const char* notepad_menu_items[] = { "Ac", "Kaydet", "Farkli Kaydet", "Kapat" };

// --- YARDIMCI FONKSİYONLAR ---

static void notepad_direct_save(notepad_t* data) {
    if (!data || strlen(data->file_path) == 0) return;

    // İçeriği tamamen sıfırlamak için önce siliyoruz
    vfs_remove(data->file_path); 

    vfs_file_t* f = NULL;
    if (vfs_open(data->file_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written;
        vfs_write(f, data->text, data->cursor, &written);
        vfs_close(f);
        
        data->is_dirty = false;
        notification_show("Kaydedildi", 1000);
    } else {
        notification_show("Hata: Kaydedilemedi!", 2000);
    }
}

static void notepad_on_save_confirm(const char* filename) {
    if (global_data) {
        strcpy(global_data->file_path, "/home/desktop/");
        strcat(global_data->file_path, filename);
        if (strstr(global_data->file_path, ".txt") == NULL) strcat(global_data->file_path, ".txt");
        
        notepad_direct_save(global_data);
    }
}

static void notepad_on_open_confirm(const char* filename) {
    char full_path[256];
    strcpy(full_path, "/home/desktop/");
    strcat(full_path, filename);
    if (strstr(full_path, ".txt") == NULL) strcat(full_path, ".txt");
    
    notepad_open_file(full_path);
}

// --- APP CALLBACK'LERİ ---

static void notepad_on_create(app_t* self) {
    notepad_t* data = (notepad_t*)self->user;
    if (data) {
        global_data = data;
        data->cursor = 0;
        data->menu_open = false;
        data->is_dirty = false;
        data->window_id = self->win_id;
        memset(data->text, 0, NOTEPAD_MAX_TEXT);
        memset(data->file_path, 0, 128);
    }
}

static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;

    if (save_dialog_is_active()) data->menu_open = false; 

    ui_rect_t client = wm_get_client_rect(self->win_id);
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();

    // 1. Menü Çubuğu
    gfx_fill_rect(client.x, client.y, client.w, 20, 0xCCCCCC);

    char header_text[160];
    const char* display_name = (strlen(data->file_path) > 0) ? data->file_path : "Adsiz";
    strcpy(header_text, "Dosya: ");
    strcat(header_text, display_name);
    if (data->is_dirty) strcat(header_text, "*");
    gfx_draw_text(client.x + 120, client.y + 5, 0x444444, header_text);

    int btn_x = client.x + 5, btn_y = client.y + 2, btn_w = 55, btn_h = 16;
    bool is_hover = (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h);
    
    if (data->menu_open) gfx_fill_rect(btn_x, btn_y, btn_w, btn_h, 0xAAAAAA);
    else if (is_hover) gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, 0xFFFFFF); 
    gfx_draw_text(btn_x + 8, btn_y + 3, 0x000000, "Dosya");

    // 2. Yazı Alanı
    int text_y = client.y + 20; 
    gfx_fill_rect(client.x, text_y, client.w, client.h - 20, 0xFFFFFF);
    gfx_draw_line(client.x, text_y, client.x + client.w, text_y, 0x808080);

    int cx = client.x + 5, cy = text_y + 5;
    char buf[2] = {0, 0};
    for (uint32_t i = 0; i < data->cursor; i++) {
        buf[0] = data->text[i];
        if (buf[0] == '\n') {
            cy += 14; cx = client.x + 5;
        } else {
            gfx_draw_text(cx, cy, 0x000000, buf);
            cx += 8;
        }
        if (cy > client.y + client.h - 14) break;
    }
    gfx_draw_text(cx, cy, 0x000000, "_");

    // 3. Dropdown Menü (4 öğe için boyutu ayarladık)
    if (data->menu_open) {
        int m_x = btn_x, m_y = client.y + 20;
        gfx_fill_rect(m_x, m_y, 110, 72, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, 110, 72, 0x000000);
        
        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 5 + (i * 16);
            if (mx >= m_x && mx <= m_x + 110 && my >= item_y && my <= item_y + 16) {
                gfx_fill_rect(m_x + 1, item_y, 108, 16, 0x000080);
                gfx_draw_text(m_x + 10, item_y + 2, 0xFFFFFF, notepad_menu_items[i]);
            } else {
                gfx_draw_text(m_x + 10, item_y + 2, 0x000000, notepad_menu_items[i]);
            }
        }
    }
}

static void notepad_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t extra1, uint8_t extra2) {
    (void)extra1; (void)extra2;
    notepad_t* data = (notepad_t*)self->user;
    ui_rect_t client = wm_get_client_rect(self->win_id);
    
    if (!(buttons & 1)) return;

    if (mx >= client.x && mx <= client.x + 60 && my >= client.y && my <= client.y + 20) {
        data->menu_open = !data->menu_open;
        return;
    } 

    if (data->menu_open) {
        int m_x = client.x + 5, m_y = client.y + 20;
        if (mx >= m_x && mx <= m_x + 110) {
            int item = (my - m_y - 5) / 16;
            
            if (item == 0) { // AÇ
                save_dialog_show("Dosya Ac", "", 0, notepad_on_open_confirm);
            } else if (item == 1) { // KAYDET
                if (strlen(data->file_path) > 0) {
                    notepad_direct_save(data);
                } else {
                    save_dialog_show("Kaydet", "", 0, notepad_on_save_confirm);
                }
            } else if (item == 2) { // FARKLI KAYDET
                save_dialog_show("Farkli Kaydet", "", 0, notepad_on_save_confirm);
            } else if (item == 3) { // KAPAT
                // appmgr_stop_app(self);
            }
        }
        data->menu_open = false;
    }
}

static void notepad_on_key(app_t* self, uint16_t scancode) {
    notepad_t* data = (notepad_t*)self->user;
    char c = kbd_scancode_to_ascii((uint8_t)scancode);
    bool changed = false;

    if (scancode == 0x1C) { // Enter
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = '\n';
            changed = true;
        }
    } else if (c == '\b') { // Backspace
        if (data->cursor > 0) {
            data->text[--data->cursor] = '\0';
            changed = true;
        }
    } else if (c >= 32 && c <= 126) {
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = c;
            changed = true;
        }
    }
    if (changed) data->is_dirty = true;
}

static void notepad_on_destroy(app_t* self) {
    if (global_data == (notepad_t*)self->user) {
        global_data = NULL;
    }
}

void notepad_open_file(const char* path) {
    app_t* self = appmgr_start_app(3); 
    if (!self || !self->user) return;

    notepad_t* data = (notepad_t*)self->user;
    global_data = data;
    strncpy(data->file_path, path, 127);

    uint32_t actual_size = 0;
    int result = vfs_read_all(path, (uint8_t*)data->text, NOTEPAD_MAX_TEXT - 1, &actual_size);

    if (result >= 0) {
        data->cursor = actual_size;
        data->text[actual_size] = '\0';
        data->is_dirty = false;
    }
}

const app_vtbl_t notepad_vtbl = {
    .on_create  = notepad_on_create,
    .on_draw    = notepad_on_draw,
    .on_key     = notepad_on_key,
    .on_mouse   = notepad_on_mouse, 
    .on_destroy = notepad_on_destroy
};