#ifndef MOUSE_PS2_H
#define MOUSE_PS2_H

#include <stdint.h>

// --- Global Koordinatlar ---
// Diğer dosyalardan (desktop.c gibi) erişebilmek için extern olarak tanımlıyoruz
extern int mouse_x;
extern int mouse_y;

/**
 * @brief PS/2 fareyi başlatır, örnekleme hızını ayarlar ve 
 * veri raporlamasını (interrupts) aktif eder.
 */
void ps2_mouse_init(void);

/**
 * @brief Donanım kesmesi (IRQ12) geldiğinde çağrılan ana işleyici.
 * Gelen ham baytları toplar ve paket haline getirir.
 */
void mouse_handler(void);

/**
 * @brief Ham PS/2 baytlarını işler ve 3 baytlık paket tamamlandığında 
 * kuyruğa (queue) ekler.
 */
void ps2_mouse_handle_byte(uint8_t data);

/**
 * @brief Fare olay kuyruğundan (event queue) bir hareket verisi çeker.
 * * @param dx Değişim miktarı X (output)
 * @param dy Değişim miktarı Y (output)
 * @param buttons Buton durumu (output)
 * @return int Veri varsa 1, kuyruk boşsa 0 döner.
 */
int ps2_mouse_pop(int* dx, int* dy, uint8_t* buttons);

/**
 * @brief (Opsiyonel) Kesme yerine sürekli kontrol (polling) yapmak 
 * gerekirse kullanılır.
 */
void ps2_mouse_poll(void);

#endif // MOUSE_PS2_H