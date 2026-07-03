#include <stdarg.h>
#include <stddef.h>
#include <lib/string.h>

int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    char* ptr = str;
    size_t written = 0;

    for (const char* p = format; *p && written < size - 1; p++) {
        if (*p != '%') {
            *ptr++ = *p;
            written++;
            continue;
        }
        p++;
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                while (*s && written < size - 1) {
                    *ptr++ = *s++;
                    written++;
                }
                break;
            }
            case 'd': {
                break;
            }
        }
    }
    *ptr = '\0';
    return written;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}