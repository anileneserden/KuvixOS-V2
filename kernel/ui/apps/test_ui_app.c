#include <app/app.h>
#include <ui/wm.h>
#include <ui/theme.h>

#include <ui/widgets/button.h>
#include <ui/widgets/input.h>
#include <ui/widgets/select.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/memory/kmalloc.h>


// 1. Struct Tanımı (En Üstte)
typedef struct {
    ui_button_t btn;
    ui_input_t  name_input; // Yeni input widget'ımız
    char        input_buffer[64]; // Yazıların saklanacağı yer
} test_ui_state_t;

// 2. Create Fonksiyonu
void test_ui_on_create(app_t* a) {
    ui_theme_bootstrap_default();

    test_ui_state_t* state = (test_ui_state_t*)kmalloc(sizeof(test_ui_state_t));
    
    // Buton (Zaten vardı)
    ui_button_init(&state->btn, 50, 50, 150, 40, "Kuvix Button");

    // Input başlatma
    // Not: input.c'deki struct yapına göre buffer'ı ve uzunluğu sıfırlıyoruz
    state->name_input.x = 50;
    state->name_input.y = 110; // Butonun biraz altında olsun
    state->name_input.w = 200;
    state->name_input.h = 30;
    state->name_input.buffer = state->input_buffer;
    state->name_input.buffer[0] = '\0';
    state->name_input.len = 0;
    state->name_input.label = "Isim Giriniz:";
    state->name_input.has_focus = 1; // Test için direkt focus verelim
    state->name_input.hidden = 0;

    a->user_data = state; 
}

// 3. Mouse Fonksiyonu (Sadece BİR tane olmalı)
void test_ui_on_mouse(app_t* a, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn) {
    if (!a || !a->user_data) return;
    
    test_ui_state_t* state = (test_ui_state_t*)a->user_data;
    (void)btn; // Buton bitmask'ını kullanmıyoruz, pr/rel yeterli.

    ui_rect_t client = wm_get_client_rect(a->win_id);
    
    // Pencere içi (local) koordinatlar
    int local_mx = mx - client.x;
    int local_my = my - client.y;
    
    // UYARI ÇÖZÜMÜ: local değişkenleri kullandığımızı belirtelim
    (void)local_mx; (void)local_my;

    // Manuel Hit-Test: Mouse butonun sınırları içinde mi?
    // Not: Butonun x/y'si zaten pencere koordinatlarına göre ayarlandığı için mx/my kullanıyoruz.
    int inside = (mx >= state->btn.x && mx <= state->btn.x + state->btn.w &&
                  my >= state->btn.y && my <= state->btn.y + state->btn.h);

    // Hover (Üzerine gelme) efektini elle set edelim
    state->btn.is_hover = inside;

    if (pr && inside) {
        state->btn.is_pressed = 1;
    } 
    
    if (rel && state->btn.is_pressed) {
        state->btn.is_pressed = 0;
        if (inside) {
            // HATA ÇÖZÜMÜ: wm_set_window_title -> wm_set_title
            wm_set_title(a->win_id, "Buton Calisiyor!");
        }
    }

    // Legacy update'i de çağıralım (içeride başka logic varsa çalışsın)
    ui_button_update(&state->btn);
}

// 4. Draw Fonksiyonu
void test_ui_on_draw(app_t* a) {
    if (!a || !a->user_data) return;
    test_ui_state_t* state = (test_ui_state_t*)a->user_data;
    ui_rect_t client = wm_get_client_rect(a->win_id);

    // Arka planı temizle
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xCCCCCC); // Açık gri pencere içi

    // Buton koordinatlarını güncelle ve çiz
    state->btn.x = client.x + 50;
    state->btn.y = client.y + 50;
    ui_button_draw(&state->btn);

    // Input koordinatlarını güncelle ve çiz
    state->name_input.x = client.x + 50;
    state->name_input.y = client.y + 110;
    ui_input_draw(&state->name_input);
}

void test_ui_on_key(app_t* a, uint16_t key) {
    test_ui_state_t* state = (test_ui_state_t*)a->user_data;
    if (!state) return;

    // 1. Backspace kontrolü (Karakter silme)
    if (key == 0x08 || key == '\b') {
        if (state->name_input.len > 0) {
            state->name_input.len--;
            state->name_input.buffer[state->name_input.len] = '\0';
        }
    } 
    // 2. Yazdırılabilir karakter kontrolü (ASCII 32-126 arası)
    else if (key >= 32 && key <= 126) {
        if (state->name_input.len < 63) { // Buffer boyutundan küçükse ekle
            state->name_input.buffer[state->name_input.len] = (char)key;
            state->name_input.len++;
            state->name_input.buffer[state->name_input.len] = '\0';
        }
    }

    // 3. EKRANI TAZELE (Kritik!)
    // Yazı yazıldığında kutunun güncellenmiş halini görmek için
    wm_draw(); 
}

// 5. VTable (Hepsini tek isim altında topla)
const app_vtbl_t test_ui_vtbl = {
    .on_create = test_ui_on_create,
    .on_draw   = test_ui_on_draw,
    .on_mouse  = test_ui_on_mouse, // Burası artık tek tanıma bakıyor
    .on_key    = test_ui_on_key,
    .on_update = 0,
    .on_destroy = 0
};