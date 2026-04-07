#ifndef KUVIX_PINEAPPLE_MAIN_HPP
#define KUVIX_PINEAPPLE_MAIN_HPP

#include <stdint.h>

// C++ tarafındaki bu fonksiyonları C'ye (session.c) tanıtıyoruz
#ifdef __cplusplus
extern "C" {
#endif

void pineapple_init(void);
void pineapple_tick(void);
void pineapple_handle_scancode(uint16_t sc);

#ifdef __cplusplus
}
#endif

#endif