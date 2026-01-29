#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdarg.h>

// Sayıları metne çevirmek için yardımcı fonksiyon
void print_int(int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        vga_putc('0'); serial_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0) {
        vga_putc(buf[i]);
        serial_putc(buf[i]);
    }
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- UTF-8 DÖNÜŞTÜRME KATMANI ---
            if (c == 0xC3) { // ü, ö, ç, Ü, Ö, Ç başlangıcı
                unsigned char next = (unsigned char)*(++p); // Bir sonraki byte'ı al ve p'yi ilerlet
                if (next == 0xBC) c = 6;       // ü
                else if (next == 0xB6) c = 4;  // ö
                else if (next == 0xA7) c = 5;  // ç (Standart UTF-8)
                else if (next == 0x87) c = 11; // Ç
                else if (next == 0x9C) c = 12; // Ü
                else if (next == 0x96) c = 10; // Ö
                else { vga_putc(0xC3); vga_putc(next); continue; } // Bilinmiyorsa ikisini de bas
            } 
            else if (c == 0xC4) { // ğ, ı, İ başlangıcı
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 1;       // ğ
                else if (next == 0x9E) c = 7;  // Ğ
                else if (next == 0xB1) c = 3;  // ı
                else if (next == 0xB0) c = 9;  // İ
                else { vga_putc(0xC4); vga_putc(next); continue; }
            } 
            else if (c == 0xC5) { // ş başlangıcı
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 2;       // ş
                else if (next == 0x9E) c = 8;  // Ş
                else { vga_putc(0xC5); vga_putc(next); continue; }
            }
            // --- DÖNÜŞTÜRME BİTİŞ ---

            vga_putc(c);
            serial_putc(c);
            continue;
        }

        // --- Format Belirleyiciler (%d, %s vb.) ---
        p++; 
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) {
                    vga_putc(*s);
                    serial_putc(*s);
                    s++;
                }
                break;
            }
            case 'd': print_int(va_arg(args, int), 10); break;
            case 'x': vga_putc('0'); vga_putc('x'); print_int(va_arg(args, int), 16); break;
            case 'c': {
                char c = (char)va_arg(args, int);
                vga_putc(c);
                serial_putc(c);
                break;
            }
            case '%': {
                vga_putc('%');
                serial_putc('%');
                break;
            }
            default: {
                // Bilinmeyen karakteri yutma, olduğu gibi bas
                vga_putc('%');
                vga_putc(*p);
                serial_putc('%');
                serial_putc(*p);
                break;
            }
        }
    }
    va_end(args);
}