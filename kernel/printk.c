#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
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
        if (c == '\n') {
            fb_console_flush();
        }
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

    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0) outc(buf[i]);
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- UTF-8 DÖNÜŞTÜRME KATMANI ---
            if (c == 0xC3) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0xBC) c = 6;
                else if (next == 0xB6) c = 4;
                else if (next == 0xA7) c = 5;
                else if (next == 0x87) c = 11;
                else if (next == 0x9C) c = 12;
                else if (next == 0x96) c = 10;
                else {
                    outc((char)0xC3);
                    outc((char)next);
                    continue;
                }
            } else if (c == 0xC4) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 1;
                else if (next == 0x9E) c = 7;
                else if (next == 0xB1) c = 3;
                else if (next == 0xB0) c = 9;
                else {
                    outc((char)0xC4);
                    outc((char)next);
                    continue;
                }
            } else if (c == 0xC5) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 2;
                else if (next == 0x9E) c = 8;
                else {
                    outc((char)0xC5);
                    outc((char)next);
                    continue;
                }
            }

            outc((char)c);
            continue;
        }

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
                print_int(va_arg(args, int), 16);
                break;

            case 'c':
                outc((char)va_arg(args, int));
                break;

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
