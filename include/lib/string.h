#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// String karşılaştırma (Aynısıysa 0 döner)
int strcmp(const char* s1, const char* s2);

// String uzunluğu
size_t strlen(const char* str);

// Hafıza kopyalama (İleride scrolling için lazım olacak)
void* memcpy(void* dest, const void* src, size_t n);

// Hafıza doldurma (Ekran temizleme vb. için)
void* memset(void* s, int c, size_t n);

#endif