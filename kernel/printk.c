#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdarg.h>
#include <stdbool.h>

// --- MERKEZİ KARAKTER BASMA ---
static void printk_putc(char c) {
    vga_putc(c);
    serial_putc(c);
}

// --- SAYI YAZDIRMA (printk için yardımcı) ---
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

// --- KSPRINTF YARDIMCI (Sayıyı buffer'a yazar) ---
static void ksprintf_itoa(unsigned int value, int base, char **buf) {
    char temp[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        *((*buf)++) = '0';
        return;
    }

    while (value > 0) {
        temp[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0) {
        *((*buf)++) = temp[i];
    }
}

// --- ANA PRINTK (Türkçe Karakter Destekli) ---
void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- TÜRKÇE KARAKTER / UTF-8 DESTEĞİ ---
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

// --- KSPRINTF GÖVDESİ ---
int ksprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *ptr = buf;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            *ptr++ = *p;
            continue;
        }

        p++; // '%' atla
        switch (*p) {
            case 's': {
                char *s = va_arg(args, char *);
                if (!s) s = "(null)";
                while (*s) *ptr++ = *s++;
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    *ptr++ = '-';
                    val = -val;
                }
                ksprintf_itoa((unsigned int)val, 10, &ptr);
                break;
            }
            case 'x':
                ksprintf_itoa(va_arg(args, unsigned int), 16, &ptr);
                break;
            case 'c':
                *ptr++ = (char)va_arg(args, int);
                break;
            default:
                *ptr++ = *p;
                break;
        }
    }
    *ptr = '\0'; // String sonlandırıcı
    va_end(args);
    return (int)(ptr - buf);
}