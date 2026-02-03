#include <kernel/printk.h>
#include <stdarg.h>
#include <lib/string.h>

// Harici donanım fonksiyonları (Kendi sistemine göre kontrol et)
extern void vga_putc(unsigned char c);
extern void serial_putc(unsigned char c);

// --- TERMINAL KANCASI (HOOK) ALTYAPISI ---
static void (*g_printk_hook)(const char*) = 0;

void printk_set_hook(void (*hook)(const char*)) {
    g_printk_hook = hook;
}

// Global geçici tampon: Her printk çağrısında metni burada toplarız
static char g_term_buf[1024];
static int  g_buf_ptr = 0;

// MAKRO: Her karakteri hem donanıma hem de Terminal tamponuna gönderir
#define PUTC_BOTH(c) { \
    unsigned char _ch = (unsigned char)(c); \
    vga_putc(_ch); \
    serial_putc(_ch); \
    if (g_printk_hook && g_buf_ptr < (int)(sizeof(g_term_buf) - 1)) { \
        g_term_buf[g_buf_ptr++] = _ch; \
    } \
}

// --- SAYI YAZDIRMA FONKSİYONU ---
void print_int(int num, int base) {
    char buf[32];
    int i = 0;

    if (num == 0) {
        PUTC_BOTH('0');
        return;
    }

    if (num < 0 && base == 10) {
        PUTC_BOTH('-');
        num = -num;
    }

    unsigned int n = (unsigned int)num;
    while (n > 0) {
        int rem = n % base;
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        n /= base;
    }

    while (i > 0) {
        PUTC_BOTH(buf[--i]);
    }
}

// --- ANA PRINTK FONKSİYONU ---
void printk(const char* fmt, ...) {
    // Her çağrıda tamponu sıfırla
    g_buf_ptr = 0;
    for(int i = 0; i < (int)sizeof(g_term_buf); i++) g_term_buf[i] = 0;

    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- UTF-8 DÖNÜŞTÜRME KATMANI ---
            if (c == 0xC3) { 
                unsigned char next = (unsigned char)*(++p);
                if (next == 0xBC) c = 6;      // ü
                else if (next == 0xB6) c = 4; // ö
                else if (next == 0xA7) c = 5; // ç
                else if (next == 0x87) c = 11;// Ç
                else if (next == 0x9C) c = 12;// Ü
                else if (next == 0x96) c = 10;// Ö
                else { 
                    PUTC_BOTH(0xC3); PUTC_BOTH(next);
                    continue; 
                }
            } 
            else if (c == 0xC4) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 1;      // ğ
                else if (next == 0x9E) c = 7; // Ğ
                else if (next == 0xB1) c = 3; // ı
                else if (next == 0xB0) c = 9; // İ
                else { 
                    PUTC_BOTH(0xC4); PUTC_BOTH(next);
                    continue; 
                }
            } 
            else if (c == 0xC5) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 2;      // ş
                else if (next == 0x9E) c = 8; // Ş
                else { 
                    PUTC_BOTH(0xC5); PUTC_BOTH(next);
                    continue; 
                }
            }

            PUTC_BOTH(c);
            continue;
        }

        p++; // '%' karakterinden sonrakine geç
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) {
                    PUTC_BOTH(*s);
                    s++;
                }
                break;
            }
            case 'd': {
                print_int(va_arg(args, int), 10);
                break;
            }
            case 'x': {
                PUTC_BOTH('0'); PUTC_BOTH('x');
                print_int(va_arg(args, int), 16);
                break;
            }
            case 'c': {
                PUTC_BOTH((char)va_arg(args, int));
                break;
            }
            case '%': {
                PUTC_BOTH('%');
                break;
            }
            default: {
                PUTC_BOTH('%');
                PUTC_BOTH(*p);
                break;
            }
        }
    }

    // --- TERMINAL'E GÖNDER ---
    if (g_printk_hook && g_buf_ptr > 0) {
        g_term_buf[g_buf_ptr] = '\0'; // String sonlandırıcı
        g_printk_hook(g_term_buf);
    }

    va_end(args);
}