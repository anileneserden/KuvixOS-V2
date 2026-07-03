#ifndef LIB_STDIO_H
#define LIB_STDIO_H

#include <stddef.h>
#include <stdarg.h>

// vsnprintf'in prototipi
int vsnprintf(char* str, size_t size, const char* format, va_list args);

// snprintf'in prototipi
int snprintf(char* str, size_t size, const char* format, ...);

#endif