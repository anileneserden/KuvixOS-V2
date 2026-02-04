#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdarg.h>
#include <stdbool.h>

// --- MERKEZİ KARAKTER BASMA ---
// Tüm çıktılar buradan geçer, böylece akışı tek noktadan yönetirsin.
static void printk_putc(char c) {
    // 1. Fiziksel VGA ekranına karakteri bas (0xB8000 adresine yazar)
    vga_putc(c);

    // 2. Seri porta karakteri bas (Hata ayıklama logları için)
    // Eğer seri portta hala çift görüyorsan, vga_putc'nin içini kontrol etmelisin; 
    // vga_putc kendi içinde serial_putc çağırıyor olabilir.
    serial_putc(c);
}

// --- SAYI YAZDIRMA (DEBUG İÇİN) ---
void print_int(unsigned int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        printk_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0) {
        printk_putc(buf[i]);
    }
}

// --- ANA PRINTK ---
void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- TÜRKÇE KARAKTER / UTF-8 DESTEĞİ ---
            // Bu kısım VGA fontundaki (CP437 veya özel) indislerle eşleşir.
            if (c == 0xC3) { 
                unsigned char next = (unsigned char)*(++p);
                if (next == 0xBC) c = 6;      // ü
                else if (next == 0xB6) c = 4; // ö
                else if (next == 0xA7) c = 5; // ç
                else if (next == 0x87) c = 11;// Ç
                else if (next == 0x9C) c = 12;// Ü
                else if (next == 0x96) c = 10;// Ö
                else { printk_putc(0xC3); printk_putc(next); continue; }
            } 
            else if (c == 0xC4) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 1;      // ğ
                else if (next == 0x9E) c = 7; // Ğ
                else if (next == 0xB1) c = 3; // ı
                else if (next == 0xB0) c = 9; // İ
                else { printk_putc(0xC4); printk_putc(next); continue; }
            } 
            else if (c == 0xC5) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 2;      // ş
                else if (next == 0x9E) c = 8; // Ş
                else { printk_putc(0xC5); printk_putc(next); continue; }
            }

            printk_putc(c);
            continue;
        }

        p++; // '%' karakterini atla
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) printk_putc(*s++);
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    printk_putc('-');
                    val = -val;
                }
                print_int((unsigned int)val, 10);
                break;
            }
            case 'x': 
                printk_putc('0'); printk_putc('x');
                print_int(va_arg(args, unsigned int), 16); 
                break;
            case 'c': 
                printk_putc((char)va_arg(args, int));
                break;
            case '%': 
                printk_putc('%');
                break;
            default:
                printk_putc('%');
                printk_putc(*p);
                break;
        }
    }
    va_end(args);
}