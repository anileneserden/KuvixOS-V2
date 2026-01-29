#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// String fonksiyonları
int    strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
size_t strlen(const char* str);

// Bellek (Hafıza) fonksiyonları
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);

#endif