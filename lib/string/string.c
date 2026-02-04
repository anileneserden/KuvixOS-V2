#include <lib/string.h>

size_t strlen(const char* s) {
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int streq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

char* strcpy(char* dst, const char* src) {
    char* temp = dst;
    while ((*dst++ = *src++));
    return temp;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

// Belirli bir karakterin dizideki ilk konumunu bulur
char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return 0;
    }
    return (char*)s;
}

// Belirli bir karakterin dizideki SON konumunu bulur (Dosya yolları için çok kritiktir)
char* strrchr(const char* s, int c) {
    char* last = 0;
    do {
        if (*s == (char)c) last = (char*)s;
    } while (*s++);
    return last;
}