#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include <stdint.h>

/**
 * @brief Debug ekranını başlatır ve kmain'den kontrolü devralır.
 */
void debug_screen_init(void);

/**
 * @brief Her karede yapılacak çizim ve mantık işlemlerini yönetir.
 */
void debug_screen_update(void);

/**
 * @brief Klavye sürücüsünden (PS/2) gelen ham scancode'u buraya iletir.
 * @param scancode Ham klavye verisi
 */
void debug_screen_on_key(uint8_t scancode);

#endif