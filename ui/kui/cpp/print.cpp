#include <ui/kui/cpp/print.hpp>
#include <stdarg.h>

extern "C" {
    void printk(const char* fmt, ...);
}

void print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // basit çözüm (geçici)
    printk(fmt, args);

    va_end(args);
}