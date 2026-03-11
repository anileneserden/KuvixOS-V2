// ui/dialogs/save_dialog.c

#include <ui/dialogs/save_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <ui/notification.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/printk.h>

// --- DIŞ BİLDİRİMLER ---
extern void desktop_icons_init(void);
extern void desktop_icons_snap_all(void);
extern void desktop_icons_reset_selection(void);
extern void desktop_reset_selection_state(void);

// ------------------------------------------------------------

#define MAX_ITEMS 14

typedef struct {
    char name[32];
    bool is_dir;
} dialog_item_t;

// Save-as-type seçenekleri
typedef struct {
    const char* label;   // ekranda görünen
    const char* ext;     // ".txt" ".json" gibi, ALL için NULL
} save_type_t;

static const save_type_t g_types[] = {
    { "Tum Dosyalar (*.*)",     NULL     },
    { "Metin Belgesi (*.txt)",  ".txt"   },
    { "JSON Dosyasi (*.json)",  ".json"  },
};
static const int g_type_count = (int)(sizeof(g_types) / sizeof(g_types[0]));

// --- STATİK DEĞİŞKENLER ---
static save_dialog_t current_dialog;
static bool is_active = false;

static char current_path[128] = "/";

static dialog_item_t items[MAX_ITEMS];
static int item_count = 0;
static int selected_item = -1;

static int  selected_type = 1;          // default: txt
static bool type_dropdown_open = false; // Save as type dropdown

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

static bool filename_has_extension(const char* name) {
    if (!name || !name[0]) return false;

    // son '/' sonrası bak
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;

    const char* dot = strrchr(base, '.');
    if (!dot) return false;
    if (dot == base) return false;    // ".bashrc" gibi -> uzantı sayma (istersen true yap)
    if (dot[1] == '\0') return false; // "file." gibi
    return true;
}

static bool filename_has_dot_ext(const char* name) {
    if (!name || !name[0]) return false;

    // sağdan: boşlukları kırp (kullanıcı "deneme.html " yazarsa)
    int len = (int)strlen(name);
    while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t')) len--;
    if (len <= 0) return false;

    // son '/' sonrası
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;

    // base'in sonunda boşluk kırpılmış len'e göre bakmak zor,
    // pratik: base pointer'ı al, base_len hesapla
    int base_len = (int)strlen(base);
    while (base_len > 0 && (base[base_len - 1] == ' ' || base[base_len - 1] == '\t')) base_len--;
    if (base_len <= 0) return false;

    // base içinde son '.' var mı?
    // - dot ilk karakter olmasın (".bashrc" gibi) -> uzantı sayma
    // - dot en sonda olmasın ("file.") -> uzantı değil
    int last_dot = -1;
    for (int i = 0; i < base_len; i++) {
        if (base[i] == '.') last_dot = i;
    }

    if (last_dot <= 0) return false;
    if (last_dot >= base_len - 1) return false;

    return true;
}

// ------------------------------------------------------------
// VFS Tarama Callback
// ------------------------------------------------------------
static int dialog_vfs_cb(const char* path, uint32_t size, void* u) {
    (void)u;
    if (item_count >= MAX_ITEMS) return 0;

    int cp_len = (int)strlen(current_path);
    if (strncmp(path, current_path, (size_t)cp_len) != 0) return 1;

    const char* relative_path = path + cp_len;
    if (relative_path[0] == '/') relative_path++;

    // Daha derindeki klasörleri filtrele
    if (strchr(relative_path, '/') != NULL) return 1;
    if (strlen(relative_path) == 0) return 1;

    strncpy(items[item_count].name, relative_path, 31);
    items[item_count].name[31] = '\0';

    // senin VFS’de size==0 klasör gibi kullanılmış
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

// ------------------------------------------------------------
// KAYIT
// ------------------------------------------------------------
static void perform_save_action(void) {
    if (strlen(current_dialog.buffer) == 0) {
        notification_show("Lutfen bir isim girin!", 2000);
        return;
    }

    // Save-as-type uzantı ekleme kuralları:
    // 1) Kullanıcı zaten ".ext" yazdıysa ASLA ekleme.
    // 2) Type "Tum Dosyalar" ise ASLA ekleme.
    // 3) Aksi halde seçilen ext'i ekle.
    const char* ext = g_types[selected_type].ext; // NULL => Tum Dosyalar

    if (ext && !filename_has_dot_ext(current_dialog.buffer)) {
        size_t bl = strlen(current_dialog.buffer);
        size_t el = strlen(ext);
        if (bl + el < sizeof(current_dialog.buffer)) {
            strcat(current_dialog.buffer, ext);
        }
    }

    char full_path[256];
    memset(full_path, 0, sizeof(full_path));
    strcpy(full_path, current_path);

    // Yol sonuna slash ekle
    int len = (int)strlen(full_path);
    if (len > 0 && full_path[len - 1] != '/') strcat(full_path, "/");

    // full_path kapasite kontrol (basit)
    if (strlen(full_path) + strlen(current_dialog.buffer) >= sizeof(full_path)) {
        notification_show("Hata: Yol cok uzun!", 3000);
        return;
    }

    strcat(full_path, current_dialog.buffer);

    vfs_file_t* f = NULL;
    if (vfs_open(full_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written = 0;

        // Dosya boyutu 0 olmasın diye en az 1 byte yaz
        const char* final_data = (current_dialog.data_size > 0) ? current_dialog.data : " ";
        uint32_t final_size    = (current_dialog.data_size > 0) ? current_dialog.data_size : 1;

        vfs_write(f, final_data, final_size, &written);
        vfs_close(f);

        notification_show("Kaydedildi!", 2000);

        if (current_dialog.on_save) current_dialog.on_save(current_dialog.buffer);

        is_active = false;
        type_dropdown_open = false;

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
        // path join
        if (strcmp(current_path, "/") != 0) {
            if (strlen(current_path) + 1 < sizeof(current_path)) strcat(current_path, "/");
        }
        if (strlen(current_path) + strlen(items[index].name) < sizeof(current_path)) {
            strcat(current_path, items[index].name);
        }
        save_dialog_refresh();
    } else {
        // Dosya seçildiyse ismini kutuya kopyala
        strncpy(current_dialog.buffer, items[index].name, 63);
        current_dialog.buffer[63] = '\0';
    }
}

// ------------------------------------------------------------
// MOUSE
// ------------------------------------------------------------
void save_dialog_handle_mouse(int mx, int my, bool clicked) {
    if (!is_active || !clicked) return;

    int dw = 400, dh = 310;
    int dx = (fb_get_width()  - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;

    // 1) Kapatma (X)
    if (mx >= dx + dw - 22 && mx <= dx + dw - 4 &&
        my >= dy + 4       && my <= dy + 20) {
        is_active = false;
        type_dropdown_open = false;
        return;
    }

    // 2) Geri Butonu
    if (mx >= dx + 15 && mx <= dx + 37 &&
        my >= dy + 30 && my <= dy + 52) {
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

    // Liste
    int list_y = dy + 60;
    int list_h = 130;

    for (int i = 0; i < item_count; i++) {
        int iy = list_y + 4 + (i * 18);
        if (mx >= dx + 15 && mx <= dx + dw - 15 &&
            my >= iy      && my <= iy + 18) {
            if (selected_item == i) save_dialog_handle_item_click(i);
            selected_item = i;
            return;
        }
    }

    // Alt alanlar
    int input_y = list_y + list_h + 15;
    int btn_x   = dx + dw - 85;

    // Save as type kutusu
    int type_y = input_y + 30;
    int type_x = dx + 90;
    int type_w = dw - 190;
    int type_h = 20;

    // type box click => dropdown toggle
    if (mx >= type_x && mx <= type_x + type_w &&
        my >= type_y && my <= type_y + type_h) {
        type_dropdown_open = !type_dropdown_open;
        return;
    }

    // dropdown açıkken seçim
    if (type_dropdown_open) {
        int dd_x = type_x;
        int dd_y = type_y + type_h;
        int dd_w = type_w;
        int dd_h = g_type_count * 18 + 4;

        if (mx >= dd_x && mx <= dd_x + dd_w &&
            my >= dd_y && my <= dd_y + dd_h) {
            int idx = (my - (dd_y + 2)) / 18;
            if (idx >= 0 && idx < g_type_count) selected_type = idx;
            type_dropdown_open = false;
            return;
        } else {
            // dışarı tıklayınca kapat
            type_dropdown_open = false;
        }
    }

    // Kaydet / İptal butonları
    if (mx >= btn_x && mx <= btn_x + 70) {
        if (my >= input_y && my <= input_y + 22) { // Kaydet
            perform_save_action();
            return;
        }
        if (my >= input_y + 30 && my <= input_y + 52) { // İptal
            is_active = false;
            type_dropdown_open = false;
            return;
        }
    }
}

// ------------------------------------------------------------
// DRAW
// ------------------------------------------------------------
void save_dialog_draw(void) {
    if (!is_active) return;

    int dw = 400, dh = 310;
    int dx = (fb_get_width()  - dw) / 2;
    int dy = (fb_get_height() - dh) / 2;
    int btn_x = dx + dw - 85;

    gfx_fill_rect(dx, dy, dw, dh, 0xC6C6C6);
    gfx_draw_rect(dx, dy, dw, dh, 0x000000);

    // Header
    gfx_fill_rect(dx + 2, dy + 2, dw - 4, 20, 0x000080);
    gfx_draw_text(dx + 8, dy + 5, 0xFFFFFF, current_dialog.title);

    // Kapatma
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

    // Input
    int input_y = list_y + list_h + 15;
    gfx_draw_text(dx + 15, input_y + 3, 0x000000, "Dosya adi:");
    gfx_fill_rect(dx + 90, input_y, dw - 190, 20, 0xFFFFFF);
    gfx_draw_rect(dx + 90, input_y, dw - 190, 20, 0x808080);
    gfx_draw_text(dx + 95, input_y + 3, 0x000000, current_dialog.buffer);

    // Save as type
    int type_y = input_y + 30;
    gfx_draw_text(dx + 15, type_y + 3, 0x000000, "Save as type:");
    gfx_fill_rect(dx + 90, type_y, dw - 190, 20, 0xFFFFFF);
    gfx_draw_rect(dx + 90, type_y, dw - 190, 20, 0x808080);
    gfx_draw_text(dx + 95, type_y + 3, 0x000000, g_types[selected_type].label);
    gfx_draw_text(dx + 90 + (dw - 190) - 12, type_y + 3, 0x000000, "v");

    // Dropdown
    if (type_dropdown_open) {
        int dd_x = dx + 90;
        int dd_y = type_y + 20;
        int dd_w = dw - 190;
        int dd_h = g_type_count * 18 + 4;

        gfx_fill_rect(dd_x, dd_y, dd_w, dd_h, 0xFFFFFF);
        gfx_draw_rect(dd_x, dd_y, dd_w, dd_h, 0x000000);

        for (int i = 0; i < g_type_count; i++) {
            int iy = dd_y + 2 + i * 18;
            if (i == selected_type) gfx_fill_rect(dd_x + 1, iy, dd_w - 2, 18, 0xCCE8FF);
            gfx_draw_text(dd_x + 5, iy + 2, 0x000000, g_types[i].label);
        }
    }

    // Butonlar
    gfx_fill_rect(btn_x, input_y, 70, 22, 0xAAAAAA);
    gfx_draw_text(btn_x + 10, input_y + 4, 0x000000, "Kaydet");

    gfx_fill_rect(btn_x, input_y + 30, 70, 22, 0xAAAAAA);
    gfx_draw_text(btn_x + 15, input_y + 34, 0x000000, "Iptal");
}

// ------------------------------------------------------------
// SHOW / KEY / ACTIVE
// ------------------------------------------------------------
void save_dialog_show(const char* title, const char* data, uint32_t size,
                      int owner_win_id, save_callback_t callback) {

    memset(&current_dialog, 0, sizeof(save_dialog_t));

    strncpy(current_dialog.title, title ? title : "Kaydet", 31);
    current_dialog.title[31] = '\0';

    if (data && data[0]) {
        strncpy(current_dialog.buffer, data, 63);
        current_dialog.buffer[63] = '\0';
    } else {
        current_dialog.buffer[0] = '\0';
    }

    current_dialog.data       = data;
    current_dialog.data_size  = size;
    current_dialog.on_save    = callback;
    current_dialog.owner_win_id = owner_win_id;

    // varsayılanlar
    selected_type = 1;           // txt
    type_dropdown_open = false;

    is_active = true;

    desktop_reset_selection_state();
    desktop_icons_reset_selection();

    save_dialog_refresh();
}

void save_dialog_handle_key(uint16_t scancode, char c) {
    if (!is_active) return;

    if (scancode == 0x1C) { // Enter
        perform_save_action();
        return;
    }

    if (scancode == 0x01) { // Escape
        is_active = false;
        type_dropdown_open = false;
        return;
    }

    if (c == '\b') { // Backspace
        int len = (int)strlen(current_dialog.buffer);
        if (len > 0) current_dialog.buffer[len - 1] = '\0';
        return;
    }

    // printable
    if (c >= 32 && c <= 126) {
        int len = (int)strlen(current_dialog.buffer);
        if (len < 63) {
            current_dialog.buffer[len] = c;
            current_dialog.buffer[len + 1] = '\0';
        }
    }
}

bool save_dialog_is_active(void) {
    return is_active;
}

int save_dialog_get_owner_win_id(void) {
    return current_dialog.owner_win_id;
}