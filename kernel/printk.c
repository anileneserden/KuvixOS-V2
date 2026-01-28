#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdarg.h> // Va_list ve va_arg için şart

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            vga_putc(*p);
            serial_putc(*p);
            continue;
        }

        p++; // '%' karakterinden sonrakine bak
        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                vga_putc(c);
                serial_putc(c);
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                while (*s) {
                    vga_putc(*s);
                    serial_putc(*s);
                    s++;
                }
                break;
            }
            // Şimdilik sadece %c ve %s yeterli
            default:
                vga_putc(*p);
                serial_putc(*p);
                break;
        }
    }
    va_end(args);
}