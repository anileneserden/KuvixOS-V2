#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include <app/app.h>

/**
 * @brief Sistemi durduran ve ekrana hata mesajı basan kritik hata fonksiyonu.
 * * Bu fonksiyon çağrıldığında:
 * 1. Kesmeler (interrupts) kapatılır.
 * 2. Ekrana (VGA veya GFX) hata mesajı basılır.
 * 3. İşlemci sonsuz döngüye sokularak sistem durdurulur.
 * * @param message Hata detayını içeren metin
 */
void panic(const char *message);

/**
 * @brief Belirli bir koşulun doğruluğunu kontrol eder, değilse panic tetikler.
 * C dilindeki standart assert yapısının kernel versiyonu.
 */
#define KERNEL_ASSERT(condition, msg) \
    if (!(condition)) { \
        panic(msg); \
    }

#endif // KERNEL_PANIC_H