#include <stdint.h>
#include <stdbool.h>
#include <lib/string.h>
#include <ui/desktop.h>
#include <ui/wm.h>
#include <ui/cursor.h>
#include <ui/theme.h>
#include <ui/select/select.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/time.h>
#include <ui/apps/settings/settings_app.h>
#include "icons/terminal/terminal.h"
#include <app/app.h>
#include <app/app_manager.h>
#include <kernel/printk.h>

#include <ui/desktop_icons/text_file.h>
#include <ui/desktop_icons/generic_file.h>

#include <kernel/fs/vfs.h>

// --- EXTERN TANIMLAR (Hataları çözen kısım) ---
extern int kbd_is_ctrl_pressed(void); // keyboard.c'deki fonksiyonu çağırır

// Eğer keyboard.h içinde bunlar yoksa manuel tanımlayalım:
#ifndef KEY_LCTRL
#define KEY_LCTRL 0x1D
#endif
#ifndef KEY_RCTRL
#define KEY_RCTRL 0x1D // PS/2 Set 1'de sağ-sol ayrımı yoksa aynıdır
#endif

// --- 1. RENK TANIMLARI (Color.Black Tarzı Kullanım İçin) ---
#define COLOR_BLACK     0x000000
#define COLOR_WHITE     0xFFFFFF
#define COLOR_BLUE      0x0000FF
#define COLOR_KUVIX_BG  0x182838
#define COLOR_SELECTION 0x0055AA // Saydam mavi hissi veren renk

// --- 2. TİP TANIMLARI ---

typedef struct {
    int x, y;
    int w, h;
    bool visible;
} context_menu_t;

// --- 3. STATİK DEĞİŞKENLER ---
#define MAX_DESKTOP_ICONS 32
// --- BOYUT TANIMLARI ---
#define MENU_ITEM_HEIGHT 30  // 25'ten 30'a çıkardık (daha ferah)
#define MENU_WIDTH       180  // 160'tan 180'e çıkardık
static context_menu_t desktop_ctx_menu = {0, 0, MENU_WIDTH, 0, false};
static const char* ctx_items[] = { "Yeni Klasor", "Yeni Metin Dosyasi", "Terminali Ac" };
static const char* desktop_menu_items[] = { "Yenile", "Yeni Metin Dosyasi", "Terminali Ac" };
static const char* icon_menu_items[] = { "Ac", "Yeniden Adlandir", "Sil", "Ozellikler" };

static int renaming_icon_index = -1; 
static int selected_icon_index = -1; // Sağ tık ile seçilen ikon
static uint32_t desktop_bg_color = COLOR_BLACK; // Varsayılan arka plan
static char rename_buffer[32];
static int rename_ptr = 0;

static bool is_selecting = false; // Seçim karesi aktif mi?
static int sel_start_x = 0;       // İlk tıklandığı X
static int sel_start_y = 0;       // İlk tıklandığı Y
static int sel_end_x = 0;         // Şu anki fare X
static int sel_end_y = 0;         // Şu anki fare Y

static int icon_drag_offset_x = 0;
static int icon_drag_offset_y = 0;

// Menü Durumu İçin
static bool is_icon_menu = false;       // Açılan menü ikona mı ait yoksa masaüstüne mi?
static int menu_target_icon = -1;       // Hangi ikona sağ tıklandı? (İndis tutar)

// Çift Tıklama Mantığı İçin
static int last_clicked_icon_index = -1; // En son hangi ikona tıklandı?
static uint32_t last_click_time = 0;    // En son tıklama ne zaman yapıldı?

// --- 4. EXTERN VE YARDIMCILAR ---
extern int mouse_x, mouse_y;
extern volatile uint32_t g_ticks_ms; 
extern void appmgr_init(void);
extern app_t* appmgr_start_app(int app_id);
extern int kbd_has_character(void);
extern char kbd_get_char(void);
extern app_t* appmgr_get_app_by_window_id(int win_id);

typedef struct {
    char title[32];
    char message[64];
    bool visible;
    int x, y, w, h;
} messagebox_t;

static messagebox_t sys_msgbox = { "Sistem", "", false, 0, 0, 250, 120 };

// MessageBox'ı tetikleyecek fonksiyon
void show_messagebox(const char* title, const char* msg) {
    strncpy(sys_msgbox.title, title, 31);
    strncpy(sys_msgbox.message, msg, 63);
    
    // Ekranın ortasına hizala
    sys_msgbox.x = (fb_get_width() - sys_msgbox.w) / 2;
    sys_msgbox.y = (fb_get_height() - sys_msgbox.h) / 2;
    sys_msgbox.visible = true;
}

desktop_icon_t desktop_icons[MAX_DESKTOP_ICONS];
int icon_count = 0;

// Uzantıyı kontrol eden yardımcı fonksiyon
bool ends_with(const char* str, const char* suffix) {
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Dosya adına göre App ID belirleme
static void update_icon_app_type(desktop_icon_t* icon) {
    if (ends_with(icon->label, ".txt")) {
        icon->app_id = 4; // Not Defteri
    } else if (ends_with(icon->label, ".kef")) {
        icon->app_id = 5; // KEF Loader / Program Başlatıcı (ID'yi sistemine göre ayarla)
    } else {
        icon->app_id = 0; // Bilinmeyen / Özel işlem gerektiren
    }
}

// İstediğin arka plan rengi değiştirme metodu
void ui_desktop_set_bg_color(uint32_t color) {
    desktop_bg_color = color;
}

static void snap_to_grid(int* x, int* y) {
    int gx = (*x - 20) / 80;
    int gy = (*y - 40) / 80;
    if (gx < 0) { gx = 0; }
    if (gy < 0) { gy = 0; }
    *x = 20 + (gx * 80);
    *y = 40 + (gy * 80);
}

// vfs_list'in her dosya için çağıracağı "işçi" fonksiyon
static int desktop_load_callback(const char* path, uint32_t size, void* u) {
    (void)size; (void)u;
    
    // 1. Dosya adını ayıkla
    const char* filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;

    // . ve .. dizinlerini atla
    if (filename[0] == '.' || filename[0] == '\0') return 1;

    if (icon_count >= MAX_DESKTOP_ICONS) return 0;

    // 2. VFS'den dosya tipini sor
    vfs_stat_t st;
    if (vfs_stat(path, &st)) {
        printk("VFS BULDU: %s | Tip: %d\n", filename, st.type);
    } else {
        printk("VFS HATA: %s bulunamadi!\n", path);
        return 1;
    }

    // 3. Verileri Doldur
    strncpy(desktop_icons[icon_count].vfs_name, filename, 31);
    strncpy(desktop_icons[icon_count].label, filename, 31);

    // Tip Belirle
    if (st.type == VFS_T_DIR) {
        desktop_icons[icon_count].app_id = 1; // KLASÖR
    } else if (ends_with(filename, ".txt")) {
        desktop_icons[icon_count].app_id = 4; // METİN
    } else {
        desktop_icons[icon_count].app_id = 0; // GENERIC
    }

    // Pozisyon ayarla
    desktop_icons[icon_count].x = 40 + (icon_count / 5 * 90);
    desktop_icons[icon_count].y = 40 + (icon_count % 5 * 85);

    icon_count++;
    return 1;
}

void init_desktop_icons() {
    icon_count = 0;

    // Önce masaüstü dizinini hazırla
    vfs_mkdir("/home");
    vfs_mkdir("/home/desktop");

    // --- TEST VERİLERİ ---
    vfs_mkdir("/home/desktop/Projeler");      // SARI KLASÖR görünmeli (Uzantı yok)
    vfs_mkdir("/home/desktop/Resimler");      // SARI KLASÖR görünmeli
    vfs_write_all("/home/desktop/deneme.txt", (uint8_t*)"test", 4); // BEYAZ KAĞIT görünmeli

    // VFS'den klasör içeriğini tara
    vfs_list("/home/desktop", desktop_load_callback, 0);
}

void draw_desktop_icon(desktop_icon_t* icon, int mx, int my, int index) {
    // Seçili kutucuğu (Seçim karesi)
    if (icon->is_selected) {
        gfx_fill_rect(icon->x - 6, icon->y - 6, 44, 54, COLOR_SELECTION);
    }

    // --- RENDER DÖNGÜSÜ ---
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint8_t p = 0;
            uint32_t color = 0;

            // APP_ID'ye göre hangi bitmap kullanılacak?
            if (icon->app_id == 1) { 
                // KLASÖR ÇİZİMİ (Örn: folder_icon_bitmap)
                // Şimdilik basit sarı bir dikdörtgen çizelim (Test için)
                if (r > 2 && r < 18 && c > 0 && c < 19) color = 0xFFCC00; // Sarı
            } 
            else if (icon->app_id == 4) {
                // METİN DOSYASI (BEYAZ KAĞIT)
                p = text_file_icon[r][c];
                if (p == 1)      color = 0x000000; // Kenarlık
                else if (p == 2) color = 0xFFFFFF; // Kağıt beyazı
                else if (p == 3) color = 0xAAAAAA; // Çizgiler
            }
            else if (icon->app_id == 99) {
                // KISAYOL (Mavi dosya gibi)
                p = generic_file_icon[r][c];
                if (p == 1)      color = 0x000000;
                else if (p == 2) color = 0x00AAFF; 
            }
            else {
                // DİĞER (GENERIC)
                p = generic_file_icon[r][c];
                if (p == 1)      color = 0x000000;
                else if (p == 2) color = 0xCCCCCC;
            }

            if (color != 0) {
                fb_putpixel(icon->x + c, icon->y + r, color);
            }
        }
    }

    // --- KISAYOL OKU (OVERLAY) ---
    if (icon->app_id == 99) {
        gfx_fill_rect(icon->x, icon->y + 14, 6, 6, 0xFFFFFF); // Beyaz kutu
        fb_putpixel(icon->x + 2, icon->y + 17, 0x0000FF);    // Mavi ok ucu
    }

    // --- ETİKET ÇİZİMİ ---
    gfx_draw_text(icon->x - 10, icon->y + 25, COLOR_WHITE, icon->label);
}

void ui_desktop_run(void) {
    int dx, dy;
    uint8_t btn, last_btn = 0;

    wm_init();
    appmgr_init();
    init_desktop_icons(); 
    show_messagebox("Hos Geldiniz", "KuvixOS v2 Masaustu Hazir!");
    
    while(1) {
        ps2_mouse_poll();

        // --- 1. KLAVYE (RENAME) İŞLEMLERİ ---
        if (kbd_has_character() && renaming_icon_index != -1) {
            char c = kbd_get_char();
            if (c == '\n' || c == '\r') {
                if (rename_ptr > 0) strcpy(desktop_icons[renaming_icon_index].label, rename_buffer); 
                update_icon_app_type(&desktop_icons[renaming_icon_index]);
                renaming_icon_index = -1;
            }
            else if (c == '\b' || c == 127) {
                if (rename_ptr > 0) rename_buffer[--rename_ptr] = '\0';
            } 
            else if (rename_ptr < 20 && c >= 32 && c <= 126) {
                rename_buffer[rename_ptr++] = c;
                rename_buffer[rename_ptr] = '\0';
            }
        }

        while (ps2_mouse_pop(&dx, &dy, &btn)) {
            mouse_x += dx; mouse_y += dy;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int)fb_get_width() - 2) mouse_x = (int)fb_get_width() - 2;
            if (mouse_y > (int)fb_get_height() - 2) mouse_y = (int)fb_get_height() - 2;

            uint8_t pressed = btn & ~last_btn;
            uint8_t released = last_btn & ~btn;
            bool ctrl = kbd_is_ctrl_pressed();
            
            wm_handle_mouse_move(mouse_x, mouse_y);
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);

            // --- 2. SAĞ TIK MANTIĞI ---
            if (pressed & 2) {
                bool hit = false;
                for (int i = 0; i < icon_count; i++) {
                    if (mouse_x >= desktop_icons[i].x && mouse_x <= desktop_icons[i].x + 32 &&
                        mouse_y >= desktop_icons[i].y && mouse_y <= desktop_icons[i].y + 32) {
                        
                        if (!desktop_icons[i].is_selected) {
                            if (!ctrl) {
                                for (int j = 0; j < icon_count; j++) desktop_icons[j].is_selected = false;
                            }
                            desktop_icons[i].is_selected = true;
                        }
                        menu_target_icon = i;
                        is_icon_menu = true;
                        hit = true;
                        break;
                    }
                }
                if (!hit) {
                    is_icon_menu = false;
                    menu_target_icon = -1;
                    if (!ctrl) {
                        for (int j = 0; j < icon_count; j++) desktop_icons[j].is_selected = false;
                    }
                }
                
                // Menü boyutlarını güncelle
                int count = is_icon_menu ? 4 : 3;
                desktop_ctx_menu.w = MENU_WIDTH;
                desktop_ctx_menu.h = count * MENU_ITEM_HEIGHT;
                desktop_ctx_menu.x = mouse_x;
                desktop_ctx_menu.y = mouse_y;
                desktop_ctx_menu.visible = true;
            }

            // --- 3. SOL TIK MANTIĞI ---
            if (pressed & 1) {
                if (sys_msgbox.visible) {
                    // "Tamam" butonuna tıklandı mı?
                    int btn_x = sys_msgbox.x + (sys_msgbox.w / 2) - 30;
                    int btn_y = sys_msgbox.y + sys_msgbox.h - 35;
                    
                    if (mouse_x >= btn_x && mouse_x <= btn_x + 60 &&
                        mouse_y >= btn_y && mouse_y <= btn_y + 25) {
                        sys_msgbox.visible = false;
                    }
                    // MessageBox açıkken masaüstüne tıklanmasını engellemek için:
                    continue; 
                }

                if (desktop_ctx_menu.visible) {
                    // Tıklanan öğeyi bul (Yeni dinamik yükseklik ile)
                    if (mouse_x >= desktop_ctx_menu.x && mouse_x <= desktop_ctx_menu.x + desktop_ctx_menu.w &&
                        mouse_y >= desktop_ctx_menu.y && mouse_y <= desktop_ctx_menu.y + desktop_ctx_menu.h) {
                        
                        int item = (mouse_y - desktop_ctx_menu.y) / MENU_ITEM_HEIGHT;
                        
                        // Eski menü mantık kodların buraya gelecek (Sil, Yeni Klasör vb.)
                    }
                    desktop_ctx_menu.visible = false;
                } 
                else {
                    bool hit = false;
                    for (int i = 0; i < icon_count; i++) {
                        if (mouse_x >= desktop_icons[i].x && mouse_x <= desktop_icons[i].x + 32 &&
                            mouse_y >= desktop_icons[i].y && mouse_y <= desktop_icons[i].y + 32) {
                            
uint32_t current_time = g_ticks_ms;
                            
                            // --- ÇİFT TIKLAMA KONTROLÜ ---
                            if (i == last_clicked_icon_index && (current_time - last_click_time) < 500) {
                                const char* label = desktop_icons[i].label;
                                
                                printk("Cift Tiklandi: %s\n", label);

                                if (ends_with(label, ".exe") || ends_with(label, ".bin")) {
                                    show_messagebox("Sistem Hatasi", "Bu format desteklenmiyor!");
                                } else {
                                    appmgr_start_app(desktop_icons[i].app_id);
                                }
                                
                                // Çift tıklama işlendi, değerleri sıfırla (ÖNEMLİ!)
                                last_click_time = 0; 
                                last_clicked_icon_index = -1;
                            } 
                            else {
                                // Çift tıklama değilse, bu ilk tıklamadır; zamanı şimdi kaydet
                                last_click_time = current_time;
                                last_clicked_icon_index = i;
                            }

                            // --- SEÇİM VE SÜRÜKLEME ---
                            if (ctrl) {
                                desktop_icons[i].is_selected = !desktop_icons[i].is_selected;
                            } else {
                                if (!desktop_icons[i].is_selected) {
                                    for (int j = 0; j < icon_count; j++) desktop_icons[j].is_selected = false;
                                    desktop_icons[i].is_selected = true;
                                }
                            }

                            desktop_icons[i].dragging = true;
                            icon_drag_offset_x = mouse_x - desktop_icons[i].x;
                            icon_drag_offset_y = mouse_y - desktop_icons[i].y;
                            hit = true;
                            break;
                        }
                    }
                    if (!hit) {
                        if (!ctrl) {
                            for (int j = 0; j < icon_count; j++) desktop_icons[j].is_selected = false;
                        }
                        is_selecting = true;
                        sel_start_x = mouse_x; sel_start_y = mouse_y;
                        sel_end_x = mouse_x;   sel_end_y = mouse_y;
                    }
                }
            }

            if (released & 1) {
                if (is_selecting) {
                    int x1 = (sel_start_x < sel_end_x) ? sel_start_x : sel_end_x;
                    int y1 = (sel_start_y < sel_end_y) ? sel_start_y : sel_end_y;
                    int x2 = (sel_start_x > sel_end_x) ? sel_start_x : sel_end_x;
                    int y2 = (sel_start_y > sel_end_y) ? sel_start_y : sel_end_y;

                    for (int i = 0; i < icon_count; i++) {
                        int ix = desktop_icons[i].x + 16;
                        int iy = desktop_icons[i].y + 16;
                        if (ix >= x1 && ix <= x2 && iy >= y1 && iy <= y2) {
                            desktop_icons[i].is_selected = true;
                        }
                    }
                }
                is_selecting = false;
                for (int i = 0; i < icon_count; i++) {
                    if (desktop_icons[i].dragging) {
                        snap_to_grid(&desktop_icons[i].x, &desktop_icons[i].y);
                        desktop_icons[i].dragging = false;
                    }
                }
            }

            if (btn & 1) {
                if (is_selecting) {
                    sel_end_x = mouse_x;
                    sel_end_y = mouse_y;
                } else {
                    for (int i = 0; i < icon_count; i++) {
                        if (desktop_icons[i].dragging) {
                            for (int j = 0; j < icon_count; j++) {
                                if (desktop_icons[j].is_selected) {
                                    desktop_icons[j].x += dx;
                                    desktop_icons[j].y += dy;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            last_btn = btn;
        }

        // --- 5. ÇİZİM ---
        fb_clear(desktop_bg_color); 
        for (int i = 0; i < icon_count; i++) {
            draw_desktop_icon(&desktop_icons[i], mouse_x, mouse_y, i);
        }
        wm_draw();

        if (is_selecting) {
            int x = (sel_start_x < sel_end_x) ? sel_start_x : sel_end_x;
            int y = (sel_start_y < sel_end_y) ? sel_start_y : sel_end_y;
            int w = (sel_start_x < sel_end_x) ? (sel_end_x - sel_start_x) : (sel_start_x - sel_end_x);
            int h = (sel_start_y < sel_end_y) ? (sel_end_y - sel_start_y) : (sel_start_y - sel_end_y);

            gfx_draw_alpha_rect(w, h, 0, 85, 170, 150, x, y);
            gfx_fill_rect(x, y, w, 1, 0x00AAFF);
            gfx_fill_rect(x, y + h, w, 1, 0x00AAFF);
            gfx_fill_rect(x, y, 1, h, 0x00AAFF);
            gfx_fill_rect(x + w, y, 1, h, 0x00AAFF);
        }

        // --- YENİLENEN MENÜ ÇİZİMİ ---
        if (desktop_ctx_menu.visible) {
            // Menü Arka Planı (Koyu gri)
            gfx_fill_rect(desktop_ctx_menu.x, desktop_ctx_menu.y, desktop_ctx_menu.w, desktop_ctx_menu.h, 0x1A1A1A);
            // Menü Çerçevesi
            gfx_fill_rect(desktop_ctx_menu.x, desktop_ctx_menu.y, desktop_ctx_menu.w, 1, 0x333333);
            
            const char** items = is_icon_menu ? icon_menu_items : desktop_menu_items;
            int count = is_icon_menu ? 4 : 3;

            for (int i = 0; i < count; i++) {
                int item_y = desktop_ctx_menu.y + (i * MENU_ITEM_HEIGHT);
                
                // HOVER EFEKTİ: Fare üzerindeyse mavi vurgu yap
                if (mouse_x >= desktop_ctx_menu.x && mouse_x <= desktop_ctx_menu.x + desktop_ctx_menu.w &&
                    mouse_y >= item_y && mouse_y <= item_y + MENU_ITEM_HEIGHT) {
                    gfx_fill_rect(desktop_ctx_menu.x, item_y, desktop_ctx_menu.w, MENU_ITEM_HEIGHT, 0x0055AA);
                }

                gfx_draw_text(desktop_ctx_menu.x + 12, item_y + 10, 0xFFFFFF, items[i]);
            }
        }

        // --- MESSAGEBOX ÇİZİMİ ---
if (sys_msgbox.visible) {
    int x = sys_msgbox.x;
    int y = sys_msgbox.y;
    int w = sys_msgbox.w;
    int h = sys_msgbox.h;

    // Arka plan ve gölge efekti (hafif ofsetli koyu dikdörtgen)
    gfx_fill_rect(x + 4, y + 4, w, h, 0x22000000); 
    gfx_fill_rect(x, y, w, h, 0xFFDDDDDD); // Ana gövde
    
    // Başlık çubuğu (Koyu mavi/gri)
    gfx_fill_rect(x, y, w, 25, 0xFF0055AA);
    gfx_draw_text(x + 10, y + 5, 0xFFFFFFFF, sys_msgbox.title);
    
    // Mesaj metni
    gfx_draw_text(x + 20, y + 50, 0xFF000000, sys_msgbox.message);
    
    // "Tamam" Butonu
    int btn_x = x + (w / 2) - 30;
    int btn_y = y + h - 35;
    gfx_fill_rect(btn_x, btn_y, 60, 25, 0xFFBBBBBB);
    gfx_draw_text(btn_x + 10, btn_y + 5, 0xFF000000, "Tamam");
}

        cursor_draw_arrow(mouse_x, mouse_y);
        fb_present();
    }
}