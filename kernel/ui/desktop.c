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

#define abs(x)  ((x) < 0 ? -(x) : (x))
#define min(a,b) ((a) < (b) ? (a) : (b))

// --- Statik Masaüstü Durumu ---
static uint32_t desktop_bg_color = 0x182838;
static bool is_selecting = false;
static int sel_start_x, sel_start_y;

// 1. Sağ tık menüsünden dosya açma
void desktop_handle_open(void) {
    int hit = desktop_icons_get_hit(mouse_x, mouse_y);
    if (hit != -1) {
        desktop_icons_process_click(hit);
    }
}

// 2. Sağ tık menüsünden yeni dosya oluşturma
void desktop_handle_create_file(void) {
    vfs_file_t* f = NULL;
    const char* path = "/home/desktop/yeni_not.txt";
    
    if (vfs_open(path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        vfs_write(f, " ", 1, NULL);
        vfs_close(f);
        desktop_icons_init(); 
        desktop_icons_snap_all();
        notification_show("Dosya olusturuldu", 200);
    } else {
        notification_show("Hata: Dosya olusturulamadi", 300);
    }
}

// 3. Sağ tık menüsünden yeni klasör oluşturma
void desktop_handle_create_dir(void) {
    const char* path = "/home/desktop/yeni_klasor";
    if (vfs_mkdir(path) == 1) {
        desktop_icons_init(); 
        desktop_icons_snap_all();
        notification_show("Klasor olusturuldu", 200);
    } else {
        notification_show("Klasor zaten var", 300);
    }
}

void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn, last_btn = 0;

    // --- 1. SİSTEM BAŞLATMA ---
    wm_init();
    appmgr_init();
    topbar_init();

    // --- 2. VFS HAZIRLIĞI ---
    vfs_mkdir("/home");
    vfs_mkdir("/home/desktop");

    // Test dosyası
    vfs_file_t* test_f = NULL;
    if (vfs_open("/home/desktop/merhaba.txt", VFS_O_CREAT | VFS_O_WRONLY, &test_f) == 1) {
        uint32_t n;
        const char* greeting = "KuvixOS v2.0 Desktop Ready!";
        vfs_write(test_f, greeting, strlen(greeting), &n);
        vfs_close(test_f);
    }

    // --- 3. İKONLARI YÜKLE ---
    desktop_icons_init();
    desktop_icons_snap_all();

    // --- 4. UYGULAMAYI BAŞLAT ---
    appmgr_start_app(3); // Notepad'i başlat (Registry'deki ID: 3)

    MessageBox.Show("KuvixOS v2", "Sistem basariyla yuklendi.", MB_ICON_INFO, MessageBoxButtons.OK);

    // --- 5. ANA DÖNGÜ ---
    while(1) {
        // A. FARE İŞLEMLERİ
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

            if (!wm_is_any_window_captured()) {
                // Sağ tık menüsü
                if (pressed & 2) {
                    int hit = desktop_icons_get_hit(mouse_x, mouse_y);
                    if (hit != -1) {
                        desktop_icon_t* icon = desktop_icons_get_at(hit);
                        if (icon && !icon->is_selected) {
                            desktop_icons_deselect_all();
                            desktop_icons_select(hit);
                        }
                        context_menu_reset();
                        context_menu_add_item("Ac", desktop_handle_open);
                        context_menu_add_item("Sil", desktop_icons_delete_selected);
                        context_menu_show(mouse_x, mouse_y);
                    } else {
                        desktop_icons_deselect_all();
                        context_menu_reset();
                        context_menu_add_item("Yeni Metin Belgesi", desktop_handle_create_file);
                        context_menu_add_item("Yeni Klasor", desktop_handle_create_dir);
                        context_menu_show(mouse_x, mouse_y);
                    }
                }
                
                // Sol tık
                if (pressed & 1) {
                    if (context_menu_is_visible()) {
                        context_menu_handle_mouse(mouse_x, mouse_y, true);
                    } else {
                        int hit = desktop_icons_get_hit(mouse_x, mouse_y);
                        if (hit != -1) {
                            desktop_icons_deselect_all(); 
                            desktop_icons_set_dragging(hit, true);
                            is_selecting = false;
                        } else {
                            desktop_icons_deselect_all();
                            is_selecting = true;
                            sel_start_x = mouse_x; sel_start_y = mouse_y;
                        }
                    }
                }
                
                if (btn & 1) desktop_icons_move_dragging(mouse_x, mouse_y);

                if (released & 1) {
                    if (is_selecting) {
                        desktop_icons_select_in_rect(sel_start_x, sel_start_y, mouse_x, mouse_y);
                        is_selecting = false;
                    }
                    desktop_icons_stop_dragging_all();
                    desktop_icons_snap_all();
                }
            }
            last_btn = btn;
        }

        // B. ⭐ KLAVYE İŞLEMLERİ (FOCUS YÖNETİMİ)
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            // Aktif olan pencerenin sahibini bul
            int active_id = wm_get_active_id();
            app_t* active_app = appmgr_get_app_by_window_id(active_id);

            // Eğer bir uygulama aktifse ve tuş işleme fonksiyonu varsa ona gönder
            if (active_app && active_app->v && active_app->v->on_key) {
                active_app->v->on_key(active_app, sc);
            }
        }

        // --- 6. ÇİZİM (RENDERING) ---
        fb_clear(desktop_bg_color);
        
        desktop_icons_draw_all();
        topbar_draw();

        if (is_selecting) {
            gfx_draw_alpha_rect(abs(mouse_x - sel_start_x), abs(mouse_y - sel_start_y), 
                                0, 85, 170, 150, 
                                min(sel_start_x, mouse_x), min(sel_start_y, mouse_y));
        }

        wm_draw(); // Pencereleri ve uygulamaları (Notepad dahil) çizer
        
        context_menu_draw();
        messagebox_draw();
        notification_draw();
        cursor_draw_arrow(mouse_x, mouse_y);
        
        fb_present();
    }
}