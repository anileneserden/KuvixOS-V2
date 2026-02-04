#include <ui/apps/file_manager.h>
#include <ui/wm.h>
#include <ui/desktop.h> // desktop_icons dizisine erişim için
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>

// Diğer dosyalarda tanımlı olduğunu bildiriyoruz
extern bool ends_with(const char* str, const char* suffix);

// Tasarım Sabitleri
#define COLOR_SIDEBAR   0x2C2C2C
#define COLOR_LIST_BG   0x1E1E1E
#define COLOR_TEXT      0xEEEEEE
#define SIDEBAR_WIDTH   130
#define ITEM_HEIGHT     22

static int selected_item = -1;

void file_manager_on_draw(app_t* a) {
    ui_rect_t r = wm_get_client_rect(a->win_id);
    int head_h = 24; // Pencere başlık payı
    int content_y = r.y + head_h;
    int content_h = r.h - head_h;

    // 1. Arka Planları Çiz
    // Sidebar
    fb_draw_rect(r.x, content_y, SIDEBAR_WIDTH, content_h, COLOR_SIDEBAR);
    // Liste Alanı
    fb_draw_rect(r.x + SIDEBAR_WIDTH, content_y, r.w - SIDEBAR_WIDTH, content_h, COLOR_LIST_BG);
    // Ayırıcı Çizgi
    fb_draw_rect(r.x + SIDEBAR_WIDTH - 1, content_y, 1, content_h, 0x444444);

    // 2. Sidebar İçeriği
    gfx_draw_text(r.x + 10, content_y + 15, 0xAAAAAA, "HIZLI ERISIM");
    gfx_draw_text(r.x + 15, content_y + 40, 0x55AAFF, "> Masaustu");
    gfx_draw_text(r.x + 15, content_y + 65, COLOR_TEXT, "  Sistem (/)");

    // 3. Dosya Listesini Çiz (Desktop'tan çekiyoruz)
    // Dışarıdan 'icon_count' ve 'desktop_icons' dizisine ulaştığını varsayıyoruz
    extern int icon_count; 
    extern desktop_icon_t desktop_icons[];

    for (int i = 0; i < icon_count; i++) {
        int item_y = content_y + 10 + (i * ITEM_HEIGHT);
        int item_x = r.x + SIDEBAR_WIDTH + 10;

        // Seçili öğe vurgusu
        if (selected_item == i) {
            fb_draw_rect(r.x + SIDEBAR_WIDTH, item_y - 2, r.w - SIDEBAR_WIDTH, ITEM_HEIGHT, 0x0055AA);
        }

        // Dosya Tipine Göre İkon/Metin
        char* label = desktop_icons[i].label;
        if (ends_with(label, ".txt")) {
            gfx_draw_text(item_x, item_y, 0xFFFF00, "[TXT]"); // Sarı metin ikonu
        } else if (ends_with(label, ".kef") || ends_with(label, ".bin")) {
            gfx_draw_text(item_x, item_y, 0x00FF00, "[EXE]"); // Yeşil exe ikonu
        } else {
            gfx_draw_text(item_x, item_y, 0xBBBBBB, "[DAT]"); // Gri veri ikonu
        }

        // Dosya İsmi
        gfx_draw_text(item_x + 45, item_y, COLOR_TEXT, label);
    }
}

void file_manager_on_mouse(app_t* a, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)a;        // Kullanılmayanları sustur
    (void)released; // Hatayı gideren satır
    
    ui_rect_t r = wm_get_client_rect(a->win_id);
    int head_h = 24;
    extern int icon_count;
    extern desktop_icon_t desktop_icons[];

    if (pressed & 1) { // Sol tık
        // Liste alanında mı?
        if (mx > r.x + SIDEBAR_WIDTH) {
            int relative_y = my - (r.y + head_h + 10);
            int clicked_index = relative_y / ITEM_HEIGHT;

            if (clicked_index >= 0 && clicked_index < icon_count) {
                selected_item = clicked_index;
            } else {
                selected_item = -1;
            }
        }
    }

    // Çift Tıklama ile Dosya Açma Mantığı (Basit simülasyon)
    if (buttons & 1 && selected_item != -1) {
        // Burada desktop.c'deki çift tık mantığını çağırabilirsin
    }
}

app_vtbl_t file_manager_vtbl = {
    .on_create   = 0,
    .on_destroy  = 0,
    .on_mouse    = file_manager_on_mouse,
    .on_key      = 0,
    .on_update   = 0,
    .on_draw     = file_manager_on_draw 
};