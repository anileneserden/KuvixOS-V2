#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <kernel/drivers/video/fb_console.h>

static bool gui_mode_enabled = true;

void printk_set_gui_mode(bool enable) {
    gui_mode_enabled = enable;
}

static inline void outc(char c) {
    vga_putc(c);
    serial_putc(c);

    if (gui_mode_enabled) {
        fb_console_putc(c);
        if (c == '\n')
            fb_console_flush();
    }
}

static void print_int(int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        outc('0');
        return;
    }

    if (value < 0 && base == 10) {
        outc('-');
        value = -value;
    }

    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0)
        outc(buf[i]);
}

static void print_uint(unsigned int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) { outc('0'); return; }

    while (value > 0) {
        buf[i++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    }

    while (i--) outc(buf[i]);
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p; p++) {

        if (*p != '%') {

            unsigned char c = (unsigned char)*p;

            // ---- UTF-8 Türkçe dönüştürme ----
            if (c == 0xC3) {
                unsigned char next = (unsigned char)*(++p);

                if (next == 0xBC) c = 0xFC;      // ü
                else if (next == 0x9C) c = 0xDC; // Ü
                else if (next == 0xB6) c = 0xF6; // ö
                else if (next == 0x96) c = 0xD6; // Ö
                else if (next == 0xA7) c = 0xE7; // ç
                else if (next == 0x87) c = 0xC7; // Ç
                else if (next == 0xA9) c = 0xE9; // é
                else {
                    outc(0xC3);
                    outc(next);
                    continue;
                }
            }
            else if (c == 0xC4) {
                unsigned char next = (unsigned char)*(++p);

                if (next == 0x9F) c = 0xF0;      // ğ
                else if (next == 0x9E) c = 0xD0; // Ğ
                else if (next == 0xB1) c = 0xFD; // ı
                else if (next == 0xB0) c = 0xDD; // İ
                else {
                    outc(0xC4);
                    outc(next);
                    continue;
                }
            }
            else if (c == 0xC5) {
                unsigned char next = (unsigned char)*(++p);

                if (next == 0x9F) c = 0xFE;      // ş
                else if (next == 0x9E) c = 0xDE; // Ş
                else {
                    outc(0xC5);
                    outc(next);
                    continue;
                }
            }

            outc((char)c);
            continue;
        }

        // ---- FORMAT ----
        p++;

        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) outc(*s++);
                break;
            }

            case 'd':
                print_int(va_arg(args, int), 10);
                break;

            case 'x':
                outc('0'); outc('x');
                print_uint(va_arg(args, unsigned int), 16);
                break;

            case 'c':
                outc((char)va_arg(args, int));
                break;

            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint(v, 10);   // yoksa print_int benzeri unsigned versiyon yaz
                break;
            }

            case 'p': {
                uintptr_t pv = (uintptr_t)va_arg(args, void*);
                outc('0'); outc('x');
                print_uint((uint32_t)pv, 16);
                break;
            }

            case '%':
                outc('%');
                break;

            default:
                outc('%');
                outc(*p);
                break;
        }
    }

    va_end(args);
}
