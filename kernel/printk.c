#include <kernel/printk.h>
#include <kernel/vga.h>
#include <kernel/serial.h>
#include <stdarg.h>
#include <stdbool.h> // bool için

// Terminal fonksiyonunu dışarıdan alıyoruz
void terminal_putc(char c);
static bool gui_mode_enabled = true;

void terminal_putc(char c) {
    serial_putc(c); // Çekirdek loglarını seri porta yönlendirir
}

// GUI modunu açıp kapatmak için bir yardımcı fonksiyon
void printk_set_gui_mode(bool enable) {
    gui_mode_enabled = enable;
}

void print_int(int value, int base) {
    char buf[32];
    int i = 0;
    char *digits = "0123456789ABCDEF";

    if (value == 0) {
        vga_putc('0'); serial_putc('0');
        if (gui_mode_enabled) terminal_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (--i >= 0) {
        vga_putc(buf[i]);
        serial_putc(buf[i]);
        if (gui_mode_enabled) terminal_putc(buf[i]);
    }
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            unsigned char c = (unsigned char)*p;

            // --- UTF-8 DÖNÜŞTÜRME KATMANI (Senin kodun aynen kalıyor) ---
            if (c == 0xC3) { 
                unsigned char next = (unsigned char)*(++p);
                if (next == 0xBC) c = 6;
                else if (next == 0xB6) c = 4;
                else if (next == 0xA7) c = 5;
                else if (next == 0x87) c = 11;
                else if (next == 0x9C) c = 12;
                else if (next == 0x96) c = 10;
                else { 
                    vga_putc(0xC3); serial_putc(0xC3); if (gui_mode_enabled) terminal_putc(0xC3);
                    vga_putc(next); serial_putc(next); if (gui_mode_enabled) terminal_putc(next);
                    continue; 
                }
            } 
            else if (c == 0xC4) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 1;
                else if (next == 0x9E) c = 7;
                else if (next == 0xB1) c = 3;
                else if (next == 0xB0) c = 9;
                else { 
                    vga_putc(0xC4); serial_putc(0xC4); if (gui_mode_enabled) terminal_putc(0xC4);
                    vga_putc(next); serial_putc(next); if (gui_mode_enabled) terminal_putc(next);
                    continue; 
                }
            } 
            else if (c == 0xC5) {
                unsigned char next = (unsigned char)*(++p);
                if (next == 0x9F) c = 2;
                else if (next == 0x9E) c = 8;
                else { 
                    vga_putc(0xC5); serial_putc(0xC5); if (gui_mode_enabled) terminal_putc(0xC5);
                    vga_putc(next); serial_putc(next); if (gui_mode_enabled) terminal_putc(next);
                    continue; 
                }
            }

            vga_putc(c);
            serial_putc(c);
            if (gui_mode_enabled) terminal_putc(c);
            continue;
        }

        p++; 
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s) {
                    vga_putc(*s);
                    serial_putc(*s);
                    if (gui_mode_enabled) terminal_putc(*s);
                    s++;
                }
                break;
            }
            case 'd': print_int(va_arg(args, int), 10); break;
            case 'x': 
                vga_putc('0'); serial_putc('0'); if (gui_mode_enabled) terminal_putc('0');
                vga_putc('x'); serial_putc('x'); if (gui_mode_enabled) terminal_putc('x');
                print_int(va_arg(args, int), 16); 
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                vga_putc(c); serial_putc(c); if (gui_mode_enabled) terminal_putc(c);
                break;
            }
            case '%': {
                vga_putc('%'); serial_putc('%'); if (gui_mode_enabled) terminal_putc('%');
                break;
            }
            default: {
                vga_putc('%'); serial_putc('%'); if (gui_mode_enabled) terminal_putc('%');
                vga_putc(*p); serial_putc(*p); if (gui_mode_enabled) terminal_putc(*p);
                break;
            }
        }
    }
    va_end(args);
}