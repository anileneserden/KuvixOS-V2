#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>

// --- Sabitler ve Taslak Veriler ---
#define SIDEBAR_WIDTH 140
#define ITEM_HEIGHT    28

static const char* sidebar_links[] = { "Masaustu", "Sistem", "Belgeler", "Cöp Kutusu" };
static const char* dummy_files[]   = { "kernel.elf", "config.sys", "notlar.txt", "readme.md" };

// 1. Fonksiyon Ön Bildirimleri
void file_mgr_on_create(app_t* app);
void file_mgr_on_draw(app_t* app);
void file_mgr_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons);
void file_mgr_on_key(app_t* app, uint16_t key);

// 2. VTABLE Tanımı
const app_vtbl_t file_manager_vtbl = {
    .on_create  = file_mgr_on_create,
    .on_draw    = file_mgr_on_draw,
    .on_mouse   = file_mgr_on_mouse,
    .on_key     = file_mgr_on_key,
    .on_destroy = 0
};

// 3. Fonksiyon İçerikleri

void file_mgr_on_create(app_t* app) {
    // Uygulama ilk açıldığında yapılacak başlangıç ayarları (varsa)
    (void)app;
}

void file_mgr_on_draw(app_t* app) {
    if (!app) return;

    // Pencerenin o anki çizilebilir alanını (client area) al
    ui_rect_t client = wm_get_client_rect(app->win_id);

    // 1. Ana Arka Plan (Dosya Listesi Alanı - Beyaz)
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFFFFFFFF);

    // 2. Sidebar Arka Planı (Hafif Gri)
    gfx_fill_rect(client.x, client.y, SIDEBAR_WIDTH, client.h, 0xFFF0F0F0);
    
    // Sidebar Sağ Sınır Çizgisi
    gfx_fill_rect(client.x + SIDEBAR_WIDTH - 1, client.y, 1, client.h, 0xFFCCCCCC);

    // 3. Sidebar Öğelerini Çiz
    for (int i = 0; i < 4; i++) {
        int iy = client.y + 10 + (i * ITEM_HEIGHT);
        // Örnek: İlk öğe (Masaüstü) seçili görünsün
        if (i == 0) {
            gfx_fill_rect(client.x + 5, iy, SIDEBAR_WIDTH - 10, ITEM_HEIGHT - 4, 0xFF0055AA);
            gfx_draw_text(client.x + 15, iy + 6, 0xFFFFFFFF, sidebar_links[i]);
        } else {
            gfx_draw_text(client.x + 15, iy + 6, 0xFF333333, sidebar_links[i]);
        }
    }

    // 4. İçerik Alanı Başlıkları
    int content_start_x = client.x + SIDEBAR_WIDTH + 15;
    gfx_draw_text(content_start_x, client.y + 10, 0xFF888888, "Dosya Adi");
    gfx_draw_text(content_start_x + 180, client.y + 10, 0xFF888888, "Boyut");
    
    // Ayırıcı yatay çizgi
    gfx_fill_rect(client.x + SIDEBAR_WIDTH + 5, client.y + 28, client.w - SIDEBAR_WIDTH - 10, 1, 0xFFEEEEEE);

    // 5. Dummy Dosyaları Listele
    for (int i = 0; i < 4; i++) {
        int fy = client.y + 40 + (i * ITEM_HEIGHT);
        
        // Dosya İsmi
        gfx_draw_text(content_start_x, fy, 0xFF000000, dummy_files[i]);
        
        // Dosya Boyutu (Dummy)
        gfx_draw_text(content_start_x + 180, fy, 0xFF666666, "14 KB");
        
        // Alt çizgi (isteğe bağlı)
        gfx_fill_rect(client.x + SIDEBAR_WIDTH + 10, fy + 22, client.w - SIDEBAR_WIDTH - 20, 1, 0xFFF9F9F9);
    }
}

void file_mgr_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    // Burada ileride dosyalara tıklama ve seçme mantığı eklenecek
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

void file_mgr_on_key(app_t* app, uint16_t key) {
    // F5 yenileme veya Delete ile silme gibi tuşlar buraya gelecek
    (void)app; (void)key;
}

// File Manager için callback
static int file_mgr_list_cb(const char* path, uint32_t size, void* u) {
    app_t* app = (app_t*)u;
    // Burada dosyaları ekrana basacağız veya bir listeye alacağız.
    // Şimdilik çizim anında çağrıldığını varsayarsak:
    // (Ancak vfs_list'i on_draw içinde çağırmak performansı düşürür, 
    // en iyisi uygulama açılırken bir diziye almaktır.)
    return 1;
}