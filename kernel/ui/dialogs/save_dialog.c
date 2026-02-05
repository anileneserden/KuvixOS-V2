#include <ui/dialogs/save_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <ui/notification.h>

#define MAX_ITEMS 14

typedef struct {
    char name[32];
    int level;
    bool is_dir;
} dialog_item_t;

// --- Değişkenler ---
static save_dialog_t current_dialog;
static bool is_active = false;
static bool dropdown_open = false; 
static bool ext_dropdown_open = false; 
static char current_path[128] = "/home/desktop"; 

static dialog_item_t items[MAX_ITEMS];
static int item_count = 0;
static int selected_item = -1;

// Uzantı Seçenekleri
static const char* extensions[] = { "Tum Dosyalar (*.*)", "Metin Belgesi (*.txt)" };
static int selected_ext = 1;

// Çift tıklama simülasyonu için basit sayaç (Timer fonksiyonun yoksa iş görür)
static int last_clicked_item = -1;
static int click_timer = 0; 

// --- VFS Tarama Mantığı ---
static int dialog_vfs_cb(const char* path, uint32_t size, void* u) {
    if (item_count >= MAX_ITEMS) return 0;
    if (strcmp(path, current_path) == 0) return 1;

    int current_len = strlen(current_path);
    const char* sub_path = path + current_len;
    
    // Yol temizleme
    if (current_path[current_len-1] != '/' && *sub_path == '/') sub_path++;

    // Sadece mevcut dizinin çocuklarını göster (Torunları filtrele)
    if (strchr(sub_path, '/') != NULL) return 1;

    const char* name = strrchr(path, '/');
    if (name) name++; else name = path;
    if (strlen(name) == 0) return 1;

    strncpy(items[item_count].name, name, 31);
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

// --- Mouse İşlemleri ---
void save_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!is_active || !clicked) return;

    int dw = 320, dh = 240;
    int dx = (fb_get_width() - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;

    // 1. ÜST NAVİGASYON DROPDOWN (Konum Değiştirme)
    if (dropdown_open) {
        int menu_x = dx + 65, menu_y = dy + 46, menu_w = dw - 85;
        if (mx >= menu_x && mx <= menu_x + menu_w) {
            if (my >= menu_y + 5 && my <= menu_y + 18) strcpy(current_path, "/");
            else if (my >= menu_y + 20 && my <= menu_y + 33) strcpy(current_path, "/home");
            else if (my >= menu_y + 35 && my <= menu_y + 48) strcpy(current_path, "/home/desktop");
            
            dropdown_open = false;
            save_dialog_refresh();
            return;
        }
    }

    // 2. KONUM OK BUTONU
    if (mx >= dx + dw - 18 && mx <= dx + dw - 4 && my >= dy + 28 && my <= dy + 46) {
        dropdown_open = !dropdown_open;
        ext_dropdown_open = false;
        return;
    }

    // 3. UZANTI SEÇİCİ DROPDOWN (Kayit Türü)
    int ex_x = dx + 85, ex_y = dy + 205, ex_w = dw - 165;
    if (mx >= ex_x && mx <= ex_x + ex_w && my >= ex_y && my <= ex_y + 20) {
        ext_dropdown_open = !ext_dropdown_open;
        dropdown_open = false;
        return;
    }

    if (ext_dropdown_open) {
        if (mx >= ex_x && mx <= ex_x + ex_w) {
            if (my >= ex_y + 22 && my <= ex_y + 40) { selected_ext = 0; ext_dropdown_open = false; return; }
            if (my >= ex_y + 42 && my <= ex_y + 60) { selected_ext = 1; ext_dropdown_open = false; return; }
        }
    }

    // 4. LİSTE ÖĞELERİ (KLASÖR/DOSYA SEÇİMİ)
    int list_y = dy + 55;
    for (int i = 0; i < item_count; i++) {
        int iby = list_y + 5 + (i * 20);
        if (mx >= dx + 15 && mx <= dx + dw - 15 && my >= iby && my <= iby + 18) {
            // Çift Tıklama Mantığı
            if (last_clicked_item == i) {
                if (items[i].is_dir) {
                    if (current_path[strlen(current_path)-1] != '/') strcat(current_path, "/");
                    strcat(current_path, items[i].name);
                    save_dialog_refresh();
                    return;
                }
            }
            // Tek Tıklama Seçimi
            selected_item = i;
            last_clicked_item = i;
            if (!items[i].is_dir) strncpy(current_dialog.buffer, items[i].name, 63);
            return;
        }
    }

    // 5. KAYDET VE İPTAL BUTONLARI
    if (mx >= dx + dw - 75 && mx <= dx + dw - 10) {
        if (my >= dy + 178 && my <= dy + 198) { // Kaydet
            if (strlen(current_dialog.buffer) > 0 && current_dialog.on_save) {
                // Not: Notepad entegrasyonu için full path gönderebiliriz
                current_dialog.on_save(current_dialog.buffer);
                is_active = false;
            }
        } else if (my >= dy + 205 && my <= dy + 225) { // İptal
            is_active = false;
        }
    }
}

// --- Çizim İşlemleri ---
void save_dialog_draw(void) {
    if (!is_active) return;
    int dw = 320, dh = 240;
    int dx = (fb_get_width() - dw) / 2, dy = (fb_get_height() - dh) / 2;

    // Arka Plan ve Gölge
    gfx_fill_rect(dx + 4, dy + 4, dw, dh, 0x222222); 
    gfx_fill_rect(dx, dy, dw, dh, 0xC6C6C6);         
    gfx_draw_rect(dx, dy, dw, dh, 0x000000);         

    // Başlık
    gfx_fill_rect(dx + 2, dy + 2, dw - 4, 20, 0x000080); 
    gfx_draw_text(dx + 8, dy + 5, 0xFFFFFF, current_dialog.title);

    // Konum Alanı
    gfx_draw_text(dx + 10, dy + 30, 0x000000, "Konum:");
    gfx_fill_rect(dx + 65, dy + 28, dw - 85, 18, 0xFFFFFF);
    gfx_draw_rect(dx + 65, dy + 28, dw - 85, 18, 0x808080);
    gfx_draw_text(dx + 70, dy + 31, 0x000000, current_path);
    gfx_fill_rect(dx + dw - 18, dy + 28, 14, 18, 0xCCCCCC);
    gfx_draw_text(dx + dw - 14, dy + 31, 0x000000, "v");

    // Liste Alanı (Beyaz Kutu)
    gfx_fill_rect(dx + 10, dy + 55, dw - 20, 110, 0xFFFFFF);
    gfx_draw_rect(dx + 10, dy + 55, dw - 20, 110, 0x808080);
    
    for (int i = 0; i < item_count; i++) {
        int iby = dy + 60 + (i * 20);
        if (selected_item == i) {
            gfx_fill_rect(dx + 15, iby, dw - 30, 18, 0xCCE8FF);
            gfx_draw_rect(dx + 15, iby, dw - 30, 18, 0x0078D7);
        }
        uint32_t color = items[i].is_dir ? 0x0000AA : 0x000000;
        gfx_draw_text(dx + 20, iby + 3, color, items[i].is_dir ? "[D] " : "[F] ");
        gfx_draw_text(dx + 45, iby + 3, color, items[i].name);
    }

    // Dosya Adı Girişi
    gfx_draw_text(dx + 10, dy + 180, 0x000000, "Dosya adi:");
    gfx_fill_rect(dx + 85, dy + 178, dw - 165, 20, 0xFFFFFF);
    gfx_draw_rect(dx + 85, dy + 178, dw - 165, 20, 0x808080);
    gfx_draw_text(dx + 90, dy + 181, 0x000000, current_dialog.buffer);

    // Kayıt Türü (Uzantı Seçici)
    gfx_draw_text(dx + 10, dy + 208, 0x000000, "Kayit turu:");
    gfx_fill_rect(dx + 85, dy + 205, dw - 165, 20, 0xFFFFFF);
    gfx_draw_rect(dx + 85, dy + 205, dw - 165, 20, 0x808080);
    gfx_draw_text(dx + 90, dy + 208, 0x000000, extensions[selected_ext]);

    // Kaydet / İptal Butonları
    gfx_fill_rect(dx + dw - 75, dy + 178, 65, 20, 0xAAAAAA);
    gfx_draw_text(dx + dw - 68, dy + 181, 0x000000, "Kaydet");
    gfx_fill_rect(dx + dw - 75, dy + 205, 65, 20, 0xAAAAAA);
    gfx_draw_text(dx + dw - 68, dy + 208, 0x000000, "Iptal");

    // Dropdownlar (En üstte çizilmeli)
    if (dropdown_open) {
        int mx = dx + 65, my = dy + 46, mw = dw - 85;
        gfx_fill_rect(mx, my, mw, 50, 0xFFFFFF);
        gfx_draw_rect(mx, my, mw, 50, 0x000000);
        gfx_draw_text(mx + 5, my + 5,  0x0000FF, "/ (Root)");
        gfx_draw_text(mx + 5, my + 20, 0x0000FF, "/home");
        gfx_draw_text(mx + 5, my + 35, 0x0000FF, "/home/desktop");
    }

    if (ext_dropdown_open) {
        int ex = dx + 85, ey = dy + 226, ew = dw - 165;
        gfx_fill_rect(ex, ey, ew, 40, 0xFFFFFF);
        gfx_draw_rect(ex, ey, ew, 40, 0x000000);
        gfx_draw_text(ex + 5, ey + 5,  0x000000, extensions[0]);
        gfx_draw_text(ex + 5, ey + 25, 0x000000, extensions[1]);
    }
}

// --- Klavye ve Sistem Fonksiyonları ---
bool save_dialog_is_active(void) { return is_active; }

void save_dialog_handle_key(uint16_t scancode, char c) {
    if (!is_active) return;
    if (scancode == 0x1C) { // ENTER
        if (strlen(current_dialog.buffer) > 0 && current_dialog.on_save) {
            current_dialog.on_save(current_dialog.buffer);
            is_active = false;
        }
    } else if (scancode == 0x01) is_active = false; // ESC
    else if (c == '\b') {
        int len = strlen(current_dialog.buffer);
        if (len > 0) current_dialog.buffer[len - 1] = '\0';
    } else if (c >= 32 && c <= 126 && strlen(current_dialog.buffer) < 63) {
        int len = strlen(current_dialog.buffer);
        current_dialog.buffer[len] = c;
        current_dialog.buffer[len + 1] = '\0';
    }
}

void save_dialog_show(const char* title, save_callback_t callback) {
    memset(&current_dialog, 0, sizeof(save_dialog_t));
    strncpy(current_dialog.title, title, 31);
    current_dialog.on_save = callback;
    is_active = true;
    dropdown_open = false;
    ext_dropdown_open = false;
    save_dialog_refresh();
}