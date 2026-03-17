#ifndef KEF_LOADER_H
#define KEF_LOADER_H

#include <kernel/kef_v3.h>

/**
 * Bir bellek adresindeki KEF-v3 dosyasını çözümler, 
 * gerekli belleği ayırır ve kodu çalıştırır.
 * * @param buffer KEF dosyasının tamamının bulunduğu bellek adresi
 * @return 0: Başarılı, Negatif: Hata kodu
 */
int run_kef_v3(void* buffer);

#endif