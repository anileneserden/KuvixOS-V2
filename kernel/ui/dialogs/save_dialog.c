#include <ui/dialogs/save_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <ui/notification.h>
#include <stdint.h>
#include <stdbool.h>

// --- DIŞ BİLDİRİMLER (Linker hatalarını çözer) ---
extern void desktop_icons_init(void); 
extern void desktop_icons_snap_all(void);
extern void desktop_icons_reset_selection(void);
extern void desktop_reset_selection_state(void); // desktop.c'deki fonksiyonu tanıt

#define MAX_ITEMS 14

typedef struct {
    char name[32];
    bool is_dir;
} dialog_item_t;

// --- STATİK DEĞİŞKENLER ---
static save_dialog_t current_dialog;
static bool is_active = false;
static char current_path[128] = "/"; 

static dialog_item_t items[MAX_ITEMS];
static int item_count = 0;
static int selected_item = -1;
static const char* extensions[] = { "Tum Dosyalar (*.*)", "Metin Belgesi (*.txt)" };
static int selected_ext = 1;

// --- YARDIMCI FONKSİYONLAR ---

// VFS Tarama Callback: Sadece mevcut dizindeki öğeleri listeler
static int dialog_vfs_cb(const char* path, uint32_t size, void* u) {
    (void)u;
    if (item_count >= MAX_ITEMS) return 0;

    int cp_len = strlen(current_path);
    if (strncmp(path, current_path, cp_len) != 0) return 1;

    const char* relative_path = path + cp_len;
    if (relative_path[0] == '/') relative_path++; 

    // Daha derindeki klasörleri filtrele (Hiyerarşi kontrolü)
    if (strchr(relative_path, '/') != NULL) return 1; 

    if (strlen(relative_path) == 0) return 1;

    strncpy(items[item_count].name, relative_path, 31);
    items[item_count].is_dir = (size == 0);
    item_count++;
    return 1;
}

void save_dialog_refresh(void) {
    item_count = 0;
    selected_item = -1;
    memset(items, 0, sizeof(items));
    vfs_list(current_path, dialog_vfs_cb, NULL);
}

// --- KAYIT İŞLEMİ ---
static void perform_save_action(void) {
    if (strlen(current_dialog.buffer) == 0) {
        notification_show("Lutfen bir isim girin!", 2000);
        return;
    }

    // ⭐ OTOMATİK UZANTI EKLEME (Klasör olarak görünmeyi engeller)
    if (strstr(current_dialog.buffer, ".txt") == NULL) {
        strcat(current_dialog.buffer, ".txt");
    }

    char full_path[256];
    memset(full_path, 0, sizeof(full_path));
    strcpy(full_path, current_path);
    
    // Yol sonuna slash ekle
    int len = strlen(full_path);
    if (len > 0 && full_path[len-1] != '/') strcat(full_path, "/");
    strcat(full_path, current_dialog.buffer);

    vfs_file_t* f = NULL;
    if (vfs_open(full_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written;
        // Dosya boyutu 0 olmasın diye en az 1 byte yazıyoruz
        const char* final_data = (current_dialog.data_size > 0) ? current_dialog.data : " ";
        uint32_t final_size = (current_dialog.data_size > 0) ? current_dialog.data_size : 1;
        
        vfs_write(f, final_data, final_size, &written);
        vfs_close(f);

        notification_show("Kaydedildi!", 2000);
        
        if (current_dialog.on_save) current_dialog.on_save(current_dialog.buffer);
        
        is_active = false;
        
        // Masaüstünü tazele
        desktop_icons_init();
        desktop_icons_snap_all();
    } else {
        notification_show("Hata: Yazma basarisiz!", 3000);
    }
}

static void save_dialog_handle_item_click(int index) {
    if (index < 0 || index >= item_count) return;

    if (items[index].is_dir) {
        if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
        strcat(current_path, items[index].name);
        save_dialog_refresh(); 
    } else {
        // Dosya seçildiyse ismini kutuya kopyala
        strncpy(current_dialog.buffer, items[index].name, 63);
    }
}

// --- MOUSE İŞLEMLERİ ---
void save_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!is_active || !clicked) return;

    int dw = 400, dh = 310;
    int dx = (fb_get_width() - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;

    // 1. Kapatma (X)
    if (mx >= dx + dw - 22 && mx <= dx + dw - 4 && my >= dy + 4 && my <= dy + 20) {
        is_active = false;
        return;
    }

    // 2. Geri Butonu
    if (mx >= dx + 15 && mx <= dx + 37 && my >= dy + 30 && my <= dy + 52) {
        if (strcmp(current_path, "/") != 0) { 
            char* last_slash = strrchr(current_path, '/');
            if (last_slash != NULL) {
                if (last_slash == current_path) current_path[1] = '\0';
                else *last_slash = '\0';
                save_dialog_refresh();
            }
        }
        return;
    }

    // 4. Liste Öğeleri
    int list_y = dy + 60;
    int list_h = 130;
    for (int i = 0; i < item_count; i++) {
        int iy = list_y + 4 + (i * 18);
        if (mx >= dx + 15 && mx <= dx + dw - 15 && my >= iy && my <= iy + 18) {
            if (selected_item == i) {
                save_dialog_handle_item_click(i);
            }
            selected_item = i;
            return;
        }
    }

    // 5. Alt Butonlar (Koordinatlar Draw ile senkronize)
    int input_y = list_y + list_h + 15;
    int btn_x = dx + dw - 85;

    if (mx >= btn_x && mx <= btn_x + 70) {
        if (my >= input_y && my <= input_y + 22) { // Kaydet
            perform_save_action();
            return;
        }
        if (my >= input_y + 30 && my <= input_y + 52) { // İptal
            is_active = false;
            return;
        }
    }
}

// --- ÇİZİM İŞLEMLERİ ---
void save_dialog_draw(void) {
    if (!is_active) return;
    
    int dw = 400, dh = 310;
    int dx = (fb_get_width() - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;
    int btn_x = dx + dw - 85;

    gfx_fill_rect(dx, dy, dw, dh, 0xC6C6C6);
    gfx_draw_rect(dx, dy, dw, dh, 0x000000);
    
    gfx_fill_rect(dx + 2, dy + 2, dw - 4, 20, 0x000080);
    gfx_draw_text(dx + 8, dy + 5, 0xFFFFFF, current_dialog.title);

    // Kapatma butonu
    gfx_fill_rect(dx + dw - 22, dy + 4, 18, 16, 0xFF0000);
    gfx_draw_text(dx + dw - 16, dy + 6, 0xFFFFFF, "X");

    // Navigasyon
    gfx_fill_rect(dx + 15, dy + 30, 22, 22, 0xAAAAAA);
    gfx_draw_text(dx + 22, dy + 34, 0x000000, "<");

    gfx_fill_rect(dx + 42, dy + 30, dw - 85, 22, 0xFFFFFF); 
    gfx_draw_text(dx + 47, dy + 34, 0x000000, current_path);

    // Liste
    int list_y = dy + 60;
    int list_h = 130;
    gfx_fill_rect(dx + 15, list_y, dw - 30, list_h, 0xFFFFFF);
    gfx_draw_rect(dx + 15, list_y, dw - 30, list_h, 0x808080);
    
    for (int i = 0; i < item_count; i++) {
        int iy = list_y + 4 + (i * 18);
        if (selected_item == i) gfx_fill_rect(dx + 16, iy, dw - 32, 17, 0xCCE8FF);
        
        uint32_t color = items[i].is_dir ? 0x0000AA : 0x000000;
        gfx_draw_text(dx + 20, iy + 2, color, items[i].is_dir ? ">" : "-");
        gfx_draw_text(dx + 35, iy + 2, color, items[i].name);
    }

    // Giriş Alanları
    int input_y = list_y + list_h + 15;
    gfx_draw_text(dx + 15, input_y + 3, 0x000000, "Dosya adi:");
    gfx_fill_rect(dx + 90, input_y, dw - 190, 20, 0xFFFFFF);
    gfx_draw_text(dx + 95, input_y + 3, 0x000000, current_dialog.buffer);

    // Butonlar
    gfx_fill_rect(btn_x, input_y, 70, 22, 0xAAAAAA);
    gfx_draw_text(btn_x + 10, input_y + 4, 0x000000, "Kaydet");
    
    gfx_fill_rect(btn_x, input_y + 30, 70, 22, 0xAAAAAA);
    gfx_draw_text(btn_x + 15, input_y + 34, 0x000000, "Iptal");
}

void save_dialog_show(const char* title, const char* data, uint32_t size, save_callback_t callback) {
    memset(&current_dialog, 0, sizeof(save_dialog_t));
    strncpy(current_dialog.title, title, 31);
    current_dialog.data = data;
    current_dialog.data_size = size;
    current_dialog.on_save = callback;

    desktop_reset_selection_state();
    desktop_icons_reset_selection();

    is_active = true;
    save_dialog_refresh();
}

void save_dialog_handle_key(uint16_t scancode, char c) {
    if (!is_active) return;
    if (scancode == 0x1C) perform_save_action();
    else if (scancode == 0x01) is_active = false;
    else if (c == '\b') {
        int len = strlen(current_dialog.buffer);
        if (len > 0) current_dialog.buffer[len - 1] = '\0';
    } else if (c >= 32 && c <= 126 && strlen(current_dialog.buffer) < 63) {
        int len = strlen(current_dialog.buffer);
        current_dialog.buffer[len] = c;
        current_dialog.buffer[len + 1] = '\0';
    }
}

bool save_dialog_is_active(void) { return is_active; }