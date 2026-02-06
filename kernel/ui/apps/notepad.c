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
#include <ui/apps/notepad.h>
#include <kernel/printk.h>

#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>

// --- DIŞ BİLDİRİMLER ---
extern char kbd_scancode_to_ascii(uint8_t scancode);
extern int wm_get_mouse_x(void);
extern int wm_get_mouse_y(void);

// Callback'lerde erişebilmek için global pointer
static notepad_t* global_data = NULL;
// Menüye "Farklı Kaydet" eklendi
static const char* notepad_menu_items[] = { "Ac", "Kaydet", "Farkli Kaydet", "Kapat" };

// DEBUG: Frame sayacı
static int debug_frame_counter = 0;

// --- YARDIMCI FONKSİYONLAR ---

static void notepad_direct_save(notepad_t* data) {
    printk("[Notepad] DIRECT_SAVE called, file_path='%s'\n", data->file_path);
    
    if (!data || strlen(data->file_path) == 0) {
        printk("[Notepad] DIRECT_SAVE: Invalid data or empty path\n");
        return;
    }

    // Önce VFS (RAMFS) üzerindeki dosyayı güncelle
    printk("[Notepad] Removing old file from VFS: %s\n", data->file_path);
    vfs_remove(data->file_path); 
    
    vfs_file_t* f = NULL;
    printk("[Notepad] Creating new file in VFS\n");
    
    if (vfs_open(data->file_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written;
        vfs_write(f, data->text, data->cursor, &written);
        vfs_close(f);
        
        printk("[Notepad] VFS write successful, %u bytes written\n", written);
        
        // --- ATA DİSKE FİZİKSEL YAZMA (SYNC) ---
        if (ata_pio_is_ready()) {
            printk("[Notepad] ATA disk is ready, attempting physical write\n");
            // 1. Yol: ATA Sürücüsünü al
            blockdev_t* dev = ata_pio_get_dev();
            
            if (dev != NULL) {
                printk("[Notepad] ATA device found, writing to sector 2000\n");
                // 2. Yol: ATA'nın kendi yazma fonksiyonunu çağır (Güvenli yöntem)
                // 2000: LBA (Sektör numarası)
                // 1: Sektör sayısı (512 byte)
                // data->text: Yazılacak veri
                int result = ata_pio_drive(dev, 2000, data->text, 1);
                
                if (result) {
                    printk("[Notepad] ATA Sektör 2000 mühürlendi (SUCCESS)\n");
                } else {
                    printk("[Notepad] ATA write FAILED\n");
                }
            } else {
                printk("[Notepad] ATA device is NULL\n");
            }
        } else {
            printk("[Notepad] ATA disk NOT ready\n");
        }
        // --------------------------------------

        data->is_dirty = false;
        printk("[Notepad] Setting is_dirty = false\n");
        notification_show("Kaydedildi ve Diske Yazildi", 1000);
    } else {
        printk("[Notepad] VFS open FAILED\n");
        notification_show("Hata: Kaydedilemedi!", 2000);
    }
}

static void notepad_on_save_confirm(const char* filename) {
    printk("[Notepad] SAVE_CONFIRM callback called with filename='%s'\n", filename);
    
    if (global_data) {
        printk("[Notepad] Global data exists, current file_path='%s'\n", global_data->file_path);
        
        strcpy(global_data->file_path, "/home/desktop/");
        strcat(global_data->file_path, filename);
        if (strstr(global_data->file_path, ".txt") == NULL) {
            strcat(global_data->file_path, ".txt");
        }
        
        printk("[Notepad] New file_path='%s'\n", global_data->file_path);
        printk("[Notepad] Calling direct_save, menu_open before=%d\n", global_data->menu_open);
        
        notepad_direct_save(global_data);
        
        printk("[Notepad] After save, menu_open=%d\n", global_data->menu_open);
        // Menüyü tekrar aç
        global_data->menu_open = true;
        printk("[Notepad] Force setting menu_open=true\n");
    } else {
        printk("[Notepad] ERROR: Global data is NULL in save_confirm!\n");
    }
}

static void notepad_on_open_confirm(const char* filename) {
    printk("[Notepad] OPEN_CONFIRM callback called with filename='%s'\n", filename);
    
    char full_path[256];
    strcpy(full_path, "/home/desktop/");
    strcat(full_path, filename);
    if (strstr(full_path, ".txt") == NULL) {
        strcat(full_path, ".txt");
    }
    
    printk("[Notepad] Opening file: %s\n", full_path);
    
    // Global data'yı kontrol et ve menüyü açık tut
    if (global_data) {
        printk("[Notepad] Before open_file, menu_open=%d\n", global_data->menu_open);
        global_data->menu_open = true; // Menüyü açık tut
    }
    
    notepad_open_file(full_path);
    
    if (global_data) {
        printk("[Notepad] After open_file, menu_open=%d\n", global_data->menu_open);
    }
}

// --- APP CALLBACK'LERİ ---

static void notepad_on_create(app_t* self) {
    printk("[Notepad] ON_CREATE called, win_id=%d\n", self->win_id);
    
    notepad_t* data = (notepad_t*)self->user;
    if (data) {
        global_data = data;
        data->cursor = 0;
        data->menu_open = false;
        data->is_dirty = false;
        data->window_id = self->win_id;
        memset(data->text, 0, NOTEPAD_MAX_TEXT);
        memset(data->file_path, 0, 128);
        
        printk("[Notepad] Initialized: menu_open=%d, cursor=%d\n", 
               data->menu_open, data->cursor);
    } else {
        printk("[Notepad] ERROR: user data is NULL!\n");
    }
}

static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) {
        printk("[Notepad] ON_DRAW: self or user is NULL\n");
        return;
    }
    
    notepad_t* data = (notepad_t*)self->user;
    
    // DEBUG: Her 30 frame'de bir log
    debug_frame_counter++;
    if (debug_frame_counter % 30 == 0) {
        printk("[Notepad] ON_DRAW #%d: menu_open=%d, save_dialog_active=%d, dirty=%d\n", 
               debug_frame_counter, data->menu_open, save_dialog_is_active(), data->is_dirty);
    }
    
    // Save dialog aktifse menüyü kapat
    if (save_dialog_is_active()) {
        if (data->menu_open) {
            printk("[Notepad] Save dialog active, closing menu (was open)\n");
            data->menu_open = false;
        }
    }

    ui_rect_t client = wm_get_client_rect(self->win_id);
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();
    
    // DEBUG: Mouse pozisyonu
    if (debug_frame_counter % 60 == 0) {
        printk("[Notepad] Mouse: (%d,%d), Window: (%d,%d,%d,%d)\n", 
               mx, my, client.x, client.y, client.w, client.h);
    }

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
    
    if (data->menu_open) {
        gfx_fill_rect(btn_x, btn_y, btn_w, btn_h, 0xAAAAAA);
        if (debug_frame_counter % 60 == 0) {
            printk("[Notepad] Drawing menu button as OPEN (hover=%d)\n", is_hover);
        }
    } else if (is_hover) {
        gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, 0xFFFFFF);
        if (debug_frame_counter % 60 == 0) {
            printk("[Notepad] Drawing menu button as HOVER\n");
        }
    }
    
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
        printk("[Notepad] Drawing dropdown menu at (%d,%d)\n", btn_x, client.y + 20);
        
        int m_x = btn_x, m_y = client.y + 20;
        gfx_fill_rect(m_x, m_y, 110, 72, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, 110, 72, 0x000000);
        
        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 5 + (i * 16);
            if (mx >= m_x && mx <= m_x + 110 && my >= item_y && my <= item_y + 16) {
                gfx_fill_rect(m_x + 1, item_y, 108, 16, 0x000080);
                gfx_draw_text(m_x + 10, item_y + 2, 0xFFFFFF, notepad_menu_items[i]);
                
                if (debug_frame_counter % 30 == 0) {
                    printk("[Notepad] Menu item %d HOVER at y=%d\n", i, item_y);
                }
            } else {
                gfx_draw_text(m_x + 10, item_y + 2, 0x000000, notepad_menu_items[i]);
            }
        }
    }
}

static void notepad_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t extra1, uint8_t extra2) {
    (void)extra1; (void)extra2;
    
    printk("\n=== NOTEPAD MOUSE EVENT START ===\n");
    printk("[Notepad] MOUSE: pos=(%d,%d), buttons=0x%02X\n", mx, my, buttons);
    
    // EĞER save dialog aktifse, NOTEPAD mouse event'lerini İŞLEME!
    if (save_dialog_is_active()) {
        printk("[Notepad] Save dialog active, IGNORING mouse event\n");
        printk("=== NOTEPAD MOUSE EVENT END (ignored) ===\n");
        return;
    }
    
    // Eğer window manager başka bir pencereyi capture ettiyse
    if (wm_is_any_window_captured()) {
        printk("[Notepad] WM has captured window, ignoring\n");
        printk("=== NOTEPAD MOUSE EVENT END (captured) ===\n");
        return;
    }
    
    notepad_t* data = (notepad_t*)self->user;
    if (!data) {
        printk("[Notepad] ERROR: user data is NULL!\n");
        printk("=== NOTEPAD MOUSE EVENT END (no data) ===\n");
        return;
    }
    
    printk("[Notepad] Current state: menu_open=%d, dirty=%d, win_id=%d\n", 
           data->menu_open, data->is_dirty, self->win_id);
    
    ui_rect_t client = wm_get_client_rect(self->win_id);
    printk("[Notepad] Window rect: x=%d, y=%d, w=%d, h=%d\n", 
           client.x, client.y, client.w, client.h);
    
    // 🚨 EXTRA DEBUG: Pencerenin tam ekran koordinatlarını kontrol et
    printk("[Notepad] Mouse ABSOLUTE: mx=%d, my=%d\n", mx, my);
    printk("[Notepad] Window RELATIVE mouse: rx=%d, ry=%d\n", 
           mx - client.x, my - client.y);
    
    if (!(buttons & 1)) {
        printk("[Notepad] Not left click (buttons=0x%02X)\n", buttons);
        printk("=== NOTEPAD MOUSE EVENT END (not left) ===\n");
        return;
    }

    // Dosya butonuna tıklandı mı?
    bool file_btn_hit = (mx >= client.x && mx <= client.x + 60 && 
                         my >= client.y && my <= client.y + 20);
    
    printk("[Notepad] File button hit check: mx=%d in [%d,%d], my=%d in [%d,%d] => %d\n",
           mx, client.x, client.x + 60, my, client.y, client.y + 20, file_btn_hit);
    
    if (file_btn_hit) {
        printk("[Notepad] File button CLICKED! Toggling menu_open from %d to %d\n",
               data->menu_open, !data->menu_open);
        data->menu_open = !data->menu_open;
        printk("=== NOTEPAD MOUSE EVENT END (toggled menu) ===\n");
        return;
    }
    
    // 🚨 CRITICAL DEBUG: Menü açık mı?
    printk("[Notepad] Checking if menu is open: menu_open=%d\n", data->menu_open);
    
    // Menü açıksa ve menü alanına tıklandıysa
    if (data->menu_open) {
        int m_x = client.x + 5, m_y = client.y + 20;
        int m_w = 110, m_h = 72;
        
        bool menu_area_hit = (mx >= m_x && mx <= m_x + m_w && 
                              my >= m_y && my <= m_y + m_h);
        
        printk("[Notepad] Menu area check: mx=%d in [%d,%d], my=%d in [%d,%d] => %d\n",
               mx, m_x, m_x + m_w, my, m_y, m_y + m_h, menu_area_hit);
        
        if (menu_area_hit) {
            int item = (my - m_y - 5) / 16;
            printk("[Notepad] Menu item calculation: (my=%d - m_y=%d - 5) / 16 = %d\n",
                   my, m_y, item);
            
            if (item >= 0 && item < 4) {
                printk("[Notepad] ✅✅✅ VALID menu item %d clicked: '%s' ✅✅✅\n", 
                       item, notepad_menu_items[item]);
                
                printk("[Notepad] Closing menu (setting menu_open=false)\n");
                data->menu_open = false;
                
                // 🚨 CRITICAL: save_dialog_show çağrılıyor mu?
                if (item == 0) { // AÇ
                    printk("[Notepad] 🚀🚀🚀 ACTION: Calling save_dialog_show() 🚀🚀🚀\n");
                    printk("[Notepad] Function pointer check: %p\n", save_dialog_show);
                    
                    // Fonksiyonu çağırmadan önce son kontrol
                    if (save_dialog_show) {
                        save_dialog_show("Dosya Ac", "", 0, notepad_on_open_confirm);
                        printk("[Notepad] ✅ save_dialog_show() called successfully\n");
                        printk("[Notepad] Immediate is_active check: %d\n", save_dialog_is_active());
                    } else {
                        printk("[Notepad] ❌❌❌ save_dialog_show is NULL! ❌❌❌\n");
                    }
                } 
                // ... diğer item'lar
                
                printk("=== NOTEPAD MOUSE EVENT END (menu action) ===\n");
                return;
            } else {
                printk("[Notepad] INVALID menu item: %d (should be 0-3)\n", item);
            }
        } else {
            printk("[Notepad] Clicked OUTSIDE menu area, closing menu\n");
            data->menu_open = false;
        }
    } else {
        printk("[Notepad] Menu is not open, ignoring click\n");
    }
    
    printk("=== NOTEPAD MOUSE EVENT END ===\n");
}

static void notepad_on_key(app_t* self, uint16_t scancode) {
    notepad_t* data = (notepad_t*)self->user;
    char c = kbd_scancode_to_ascii((uint8_t)scancode);
    bool changed = false;
    
    printk("[Notepad] KEY: scancode=0x%04X, char='%c' (ASCII %d)\n", 
           scancode, (c >= 32 && c <= 126) ? c : ' ', (int)c);

    if (scancode == 0x1C) { // Enter
        printk("[Notepad] Enter pressed\n");
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = '\n';
            changed = true;
        }
    } else if (c == '\b') { // Backspace
        printk("[Notepad] Backspace pressed\n");
        if (data->cursor > 0) {
            data->text[--data->cursor] = '\0';
            changed = true;
        }
    } else if (c >= 32 && c <= 126) {
        printk("[Notepad] Character '%c' pressed\n", c);
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = c;
            changed = true;
        }
    }
    
    if (changed) {
        data->is_dirty = true;
        printk("[Notepad] Text changed, cursor=%d, dirty=true\n", data->cursor);
    }
}

static void notepad_on_destroy(app_t* self) {
    printk("[Notepad] ON_DESTROY called\n");
    
    if (global_data == (notepad_t*)self->user) {
        printk("[Notepad] Clearing global_data pointer\n");
        global_data = NULL;
    }
}

void notepad_open_file(const char* path) {
    printk("\n=== NOTEPAD OPEN_FILE START ===\n");
    printk("[Notepad] Opening file: %s\n", path);
    
    app_t* self = appmgr_start_app(3); 
    if (!self) {
        printk("[Notepad] ERROR: Could not start app\n");
        return;
    }
    
    if (!self->user) {
        printk("[Notepad] ERROR: App user data is NULL\n");
        return;
    }

    notepad_t* data = (notepad_t*)self->user;
    global_data = data;
    
    printk("[Notepad] Before loading: cursor=%d, menu_open=%d\n", 
           data->cursor, data->menu_open);
    
    strncpy(data->file_path, path, 127);
    printk("[Notepad] Set file_path to: %s\n", data->file_path);

    uint32_t actual_size = 0;
    int result = vfs_read_all(path, (uint8_t*)data->text, NOTEPAD_MAX_TEXT - 1, &actual_size);

    if (result >= 0) {
        data->cursor = actual_size;
        data->text[actual_size] = '\0';
        data->is_dirty = false;
        data->menu_open = true; // Menüyü açık tut
        
        printk("[Notepad] File loaded successfully: %u bytes, cursor=%d, menu_open=%d\n", 
               actual_size, data->cursor, data->menu_open);
    } else {
        printk("[Notepad] ERROR: Could not read file, result=%d\n", result);
        // Boş dosya oluştur
        data->cursor = 0;
        data->text[0] = '\0';
        data->is_dirty = false;
        data->menu_open = true;
    }
    
    printk("=== NOTEPAD OPEN_FILE END ===\n");
}

const app_vtbl_t notepad_vtbl = {
    .on_create  = notepad_on_create,
    .on_draw    = notepad_on_draw,
    .on_key     = notepad_on_key,
    .on_mouse   = notepad_on_mouse, 
    .on_destroy = notepad_on_destroy
};