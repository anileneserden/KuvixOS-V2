#include <lib/stdio.h>
#include <lib/string.h>
#include <stdarg.h>
#include <stddef.h>

// Yardımcı: Integer'ı string'e çevirip buffer'a yazar
static void format_int(char** ptr, size_t* written, size_t size, int value) {
    char buf[12]; // int32_t için yeterli
    int i = 0;
    
    if (value == 0) {
        if (*written < size - 1) { **ptr = '0'; (*ptr)++; (*written)++; }
        return;
    }
    
    if (value < 0) {
        if (*written < size - 1) { **ptr = '-'; (*ptr)++; (*written)++; }
        value = -value;
    }
    
    while (value > 0) {
        buf[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    while (--i >= 0 && *written < size - 1) {
        **ptr = buf[i];
        (*ptr)++;
        (*written)++;
    }
}

int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    char* ptr = str;
    size_t written = 0;

    for (const char* p = format; *p && written < size - 1; p++) {
        if (*p != '%') {
            *ptr++ = *p;
            written++;
            continue;
        }
        
        p++; // % sonrası karakter
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s && written < size - 1) {
                    *ptr++ = *s++;
                    written++;
                }
                break;
            }
            case 'd': {
                format_int(&ptr, &written, size, va_arg(args, int));
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                if (written < size - 1) { *ptr++ = c; written++; }
                break;
            }
            case '%': {
                *ptr++ = '%'; written++;
                break;
            }
            default:
                *ptr++ = *p; written++;
                break;
        }
    }
    *ptr = '\0';
    return (int)written;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}