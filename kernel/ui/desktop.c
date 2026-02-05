#include <ui/desktop.h>
#include <ui/desktop_icons.h>
#include <ui/messagebox.h>
#include <ui/wm.h>
#include <ui/cursor.h>
#include <app/app_manager.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>
#include <lib/math.h>
#include <lib/string.h>
#include <ui/notification.h>
#include <ui/topbar.h>
#include <ui/context_menu.h>
#include <kernel/fs/vfs.h>
#include <ui/apps/notepad.h>
#include <ui/dialogs/save_dialog.h>

extern char kbd_scancode_to_ascii(uint8_t scancode);

#define abs(x)  ((x) < 0 ? -(x) : (x))
#define min(a,b) ((a) < (b) ? (a) : (b))

static uint32_t desktop_bg_color = 0x182838;
static bool is_selecting = false;
static int sel_start_x, sel_start_y;

// --- Test Callback Fonksiyonu ---
void test_save_callback(const char* filename) {
    if (strlen(filename) == 0) return;

    char full_path[256];
    strcpy(full_path, "/home/desktop/");
    strcat(full_path, filename);

    vfs_file_t* f = NULL;
    // O_CREAT (oluştur) ve O_WRONLY (yaz) modunda aç
    if (vfs_open(full_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        const char* content = "KuvixOS v2 Save File Content";
        uint32_t written;
        vfs_write(f, content, strlen(content), &written);
        vfs_close(f);

        // MASAÜSTÜNÜ YENİLE
        desktop_icons_init(); 
        desktop_icons_snap_all();
        
        notification_show("Dosya kaydedildi!", 200);
    } else {
        notification_show("Hata: Kayit basarisiz!", 300);
    }
}

void desktop_handle_open(void) {
    int hit = desktop_icons_get_hit(mouse_x, mouse_y);
    if (hit != -1) desktop_icons_process_click(hit);
}

void desktop_handle_create_file(void) {
    vfs_file_t* f = NULL;
    const char* path = "/home/desktop/yeni_not.txt";
    if (vfs_open(path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        vfs_write(f, " ", 1, NULL);
        vfs_close(f);
        desktop_icons_init(); 
        desktop_icons_snap_all();
        notification_show("Dosya olusturuldu", 200);
    }
}

void desktop_handle_create_dir(void) {
    const char* path = "/home/desktop/yeni_klasor";
    if (vfs_mkdir(path) == 1) {
        desktop_icons_init(); 
        desktop_icons_snap_all();
        notification_show("Klasor olusturuldu", 200);
    }
}

void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn, last_btn = 0;

    wm_init();
    appmgr_init();
    topbar_init();

    vfs_mkdir("/home");
    vfs_mkdir("/home/desktop");

    desktop_icons_init();
    desktop_icons_snap_all();

    appmgr_start_app(3); // Notepad başlat

    MessageBox.Show("KuvixOS v2", "Sistem basariyla yuklendi.", MB_ICON_INFO, MessageBoxButtons.OK);

    while(1) {
        // --- A. FARE İŞLEMLERİ ---
        ps2_mouse_poll();
        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx; 
            mouse_y += dy;
            
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int)(fb_get_width() - 1)) mouse_x = (int)fb_get_width() - 1;
            if (mouse_y > (int)(fb_get_height() - 1)) mouse_y = (int)fb_get_height() - 1;
            
            uint8_t pressed = btn & ~last_btn;
            uint8_t released = last_btn & ~btn;

            messagebox_handle_mouse(mouse_x, mouse_y, (pressed & 1));
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);

            // --- BURAYI GÜNCELLEDİK ---
            if (save_dialog_is_active()) {
                // Senin kodunda tıklama anı 'pressed' içinde saklı
                save_dialog_handle_mouse(mouse_x, mouse_y, (pressed & 1));
                
                // Önemli: Eğer Save Dialog aktifse, tıklamanın masaüstü 
                // ikonlarına veya seçim karesine "geçmemesi" için return diyebiliriz
                // veya alttaki bloğu bir else içine alabilirsin.
            }
            // --------------------------

            if (!wm_is_any_window_captured()) {
                if (pressed & 2) {
                    int hit = desktop_icons_get_hit(mouse_x, mouse_y);
                    if (hit != -1) {
                        context_menu_reset();
                        context_menu_add_item("Ac", desktop_handle_open);
                        context_menu_add_item("Sil", desktop_icons_delete_selected);
                        context_menu_show(mouse_x, mouse_y);
                    } else {
                        context_menu_reset();
                        context_menu_add_item("Yeni Metin Belgesi", desktop_handle_create_file);
                        context_menu_add_item("Yeni Klasor", desktop_handle_create_dir);
                        context_menu_show(mouse_x, mouse_y);
                    }
                }
                if (pressed & 1) {
                    if (context_menu_is_visible()) context_menu_handle_mouse(mouse_x, mouse_y, true);
                    else {
                        int hit = desktop_icons_get_hit(mouse_x, mouse_y);
                        if (hit != -1) {
                            desktop_icons_deselect_all(); 
                            desktop_icons_set_dragging(hit, true);
                        } else {
                            desktop_icons_deselect_all();
                            is_selecting = true;
                            sel_start_x = mouse_x; sel_start_y = mouse_y;
                        }
                    }
                }
                if (btn & 1) desktop_icons_move_dragging(mouse_x, mouse_y);
                if (released & 1) {
                    if (is_selecting) desktop_icons_select_in_rect(sel_start_x, sel_start_y, mouse_x, mouse_y);
                    is_selecting = false;
                    desktop_icons_stop_dragging_all();
                    desktop_icons_snap_all();
                }
            }
            last_btn = btn;
        }

        // --- B. KLAVYE İŞLEMLERİ ---
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            // Sadece alt 8 biti (scancode) ASCII'ye çeviriyoruz
            char c = kbd_scancode_to_ascii((uint8_t)(sc & 0xFF));

            if (save_dialog_is_active()) {
                save_dialog_handle_key(sc, c);
            } 
            else {
                // F2 Scancode kontrolü (Genelde 0x3C)
                if ((sc & 0x7F) == 0x3C && !(sc & 0x80)) { 
                    save_dialog_show("Test Kayit", test_save_callback);
                } else {
                    int active_id = wm_get_active_id();
                    app_t* active_app = appmgr_get_app_by_window_id(active_id);
                    if (active_app && active_app->v && active_app->v->on_key) {
                        active_app->v->on_key(active_app, sc);
                    }
                }
            }
        }

        // --- C. ÇİZİM (RENDERING) ---
        fb_clear(desktop_bg_color);
        
        desktop_icons_draw_all();
        topbar_draw();

        if (is_selecting) {
            gfx_draw_alpha_rect(abs(mouse_x - sel_start_x), abs(mouse_y - sel_start_y), 
                                0, 85, 170, 150, 
                                min(sel_start_x, mouse_x), min(sel_start_y, mouse_y));
        }

        wm_draw(); 
        
        // Diyalog en üstte çizilmeli
        if (save_dialog_is_active()) {
            save_dialog_draw();
        }
        
        context_menu_draw();
        messagebox_draw();
        notification_draw();
        cursor_draw_arrow(mouse_x, mouse_y);
        
        fb_present();
    }
}