#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

// String fonksiyonları
int      strcmp(const char* s1, const char* s2);
int      streq(const char* s1, const char* s2); // 1: eşit, 0: değil
// String fonksiyonları altına ekle
int    strncmp(const char* s1, const char* s2, size_t n);
char* strncpy(char* dest, const char* src, size_t n);
size_t   strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);

// Bellek fonksiyonları
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);

#endif