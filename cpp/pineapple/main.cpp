#include <cpp/pineapple/main.hpp>
#include <cpp/Graphics.hpp>
#include <cpp/Framebuffer.hpp>

using namespace Kuvix;

extern "C" {

void pineapple_init(void) {
    // Pineapple Antrasit Arka Plan
    Graphics::clear(0x1A1A1A);

    uint32_t sw = Framebuffer::getWidth();
    uint32_t sh = Framebuffer::getHeight();

    // Ekranın tam ortasına şık bir karşılama metni
    // Not: drawText fonksiyonunun koordinatlarını metnin uzunluğuna göre yaklaşık ortalıyoruz
    const char* welcome_msg = "Hello, Pineapple Desktop Environment!";
    const char* sub_msg = "KuvixOS C++ Core is active.";

    Graphics::drawText((sw / 2) - 150, (sh / 2) - 10, welcome_msg, 0xFFFFFF);
    Graphics::drawText((sw / 2) - 100, (sh / 2) + 15, sub_msg, 0xAAAAAA);

    // Alt kısma küçük bir versiyon bilgisi
    Graphics::drawText(10, sh - 20, "v0.1-alpha", 0x555555);

    Framebuffer::present();
}

void pineapple_tick(void) {
    // İlk aşamada statik ekran, sadece present
    Framebuffer::present();
}

void pineapple_handle_scancode(uint16_t sc) {
    // Boş (İleride session geçişleri veya loglar için kullanılabilir)
    (void)sc; 
}

} // extern "C"