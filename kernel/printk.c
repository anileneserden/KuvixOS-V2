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

static void print_int_padded(int value, int base, int width, char pad_char, bool is_signed) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";
    bool negative = false;

    if (is_signed && value < 0 && base == 10) {
        negative = true;
        value = -value;
    }

    if (value == 0) {
        buf[i++] = '0';
    } else {
        unsigned int uv = (unsigned int)value;
        while (uv > 0) {
            buf[i++] = digits[uv % (unsigned)base];
            uv /= (unsigned)base;
        }
    }

    // Negatif işareti eklenecekse genişlik hesabına katılır
    int len = i;
    while (len < width) {
        buf[i++] = pad_char;
        len++;
    }

    if (negative) {
        buf[i++] = '-';
    }

    while (--i >= 0) {
        outc(buf[i]);
    }
}

static void print_uint_padded(unsigned int value, int base, int width, char pad_char) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            buf[i++] = digits[value % (unsigned)base];
            value /= (unsigned)base;
        }
    }

    while (i < width) {
        buf[i++] = pad_char;
    }

    while (i--) {
        outc(buf[i]);
    }
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

        // ---- FORMAT AYRIŞTIRMA (Flags & Width) ----
        p++;
        char pad_char = ' ';
        int width = 0;

        if (*p == '0') {
            pad_char = '0';
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) outc(*s++);
                break;
            }

            case 'd':
                print_int_padded(va_arg(args, int), 10, width, pad_char, true);
                break;

            case 'x':
                print_uint_padded(va_arg(args, unsigned int), 16, width, pad_char);
                break;

            case 'c':
                outc((char)va_arg(args, int));
                break;

            case 'C': {
                unsigned int fg = va_arg(args, unsigned int);
                unsigned int bg = va_arg(args, unsigned int);
                fb_console_set_color(fg, bg);
                break;
            }

            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint_padded(v, 10, width, pad_char);
                break;
            }

            case 'p': {
                uintptr_t pv = (uintptr_t)va_arg(args, void*);
                outc('0'); outc('x');
                print_uint_padded((uint32_t)pv, 16, 8, '0');
                break;
            }

            case '%':
                outc('%');
                break;

            case 'o':
                print_uint_padded(va_arg(args, unsigned int), 8, width, pad_char);
                break;

            default:
                outc('%');
                outc(*p);
                break;
        }
    }

    va_end(args);
}

// ---- KSPRINTF YARDIMCILARI ----
typedef struct {
    char* ptr;
} buf_context_t;

static inline void outc_to_buf(buf_context_t* ctx, char c) {
    *(ctx->ptr)++ = c;
    *(ctx->ptr) = '\0';
}

static void print_uint_buf(buf_context_t* ctx, unsigned int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) { 
        outc_to_buf(ctx, '0'); 
        return; 
    }

    while (value > 0) {
        buf[i++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    }

    while (i--) 
        outc_to_buf(ctx, buf[i]);
}

int ksprintf(char *buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    buf_context_t ctx = { .ptr = buf };
    char* start = buf;

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') {
            outc_to_buf(&ctx, *p);
            continue;
        }

        p++;
        while (*p >= '0' && *p <= '9') p++; // Basit atlama

        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) outc_to_buf(&ctx, *s++);
                break;
            }
            case 'd':
            case 'u':
                print_uint_buf(&ctx, (unsigned int)va_arg(args, int), 10);
                break;
            case 'x':
                outc_to_buf(&ctx, '0'); 
                outc_to_buf(&ctx, 'x');
                print_uint_buf(&ctx, va_arg(args, unsigned int), 16);
                break;
            case 'c':
                outc_to_buf(&ctx, (char)va_arg(args, int));
                break;
            case '%':
                outc_to_buf(&ctx, '%');
                break;
            default:
                outc_to_buf(&ctx, '%');
                outc_to_buf(&ctx, *p);
                break;
        }
    }

    *ctx.ptr = '\0';
    va_end(args);
    return ctx.ptr - start;
}