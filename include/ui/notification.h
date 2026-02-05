#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <stdint.h>
#include <stdbool.h>

// Bildirimi tetiklemek için fonksiyon
void notification_show(const char* text, uint32_t duration);

// Her karede çizilmesi için sistem fonksiyonu
void notification_draw(void);

#endif