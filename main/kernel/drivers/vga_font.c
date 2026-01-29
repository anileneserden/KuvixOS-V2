#include <arch/x86/io.h>
#include <stdint.h>
#include <kernel/vga_font.h>

/* 8x16 Font Bitmap Tanımları (Türkçe Küçük Harfler) */

static uint8_t font_tr_g_low[16] = {
    0b01111100, 0b00000000, 0b01111000, 0b11000100,
    0b10000100, 0b10000100, 0b11000100, 0b01111100,
    0b00000100, 0b01111000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_s_low[16] = {
    0b00000000, 0b00000000, 0b00000000, 0b00111100,
    0b01000000, 0b00111100, 0b00000100, 0b00111100,
    0b00000000, 0b00011000, 0b00001100, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_i_dotless[16] = {
    0b00000000, 0b00000000, 0b00000000, 0b00011000,
    0b00011000, 0b00011000, 0b00011000, 0b00011000,
    0b00011000, 0b00111100, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_o_low[16] = {
    0b01000100, 0b00000000, 0b00111100, 0b01000100,
    0b01000100, 0b01000100, 0b01000100, 0b00111100,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_c_low[16] = {
    0b00000000, 0b00000000, 0b00000000, 0b00111100,
    0b01000000, 0b01000000, 0b01000000, 0b00111100,
    0b00000000, 0b00011000, 0b00001100, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_u_low[16] = {
    0b01000100, 0b00000000, 0b01000100, 0b01000100,
    0b01000100, 0b01000100, 0b01000100, 0b00111110,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

/* 8x16 Font Bitmap Tanımları (Türkçe Büyük Harfler) */
static uint8_t font_tr_G_high[16] = {
    0b01111100, 0b00000000, 0b00111110, 0b01000000,
    0b01000000, 0b01000111, 0b01000001, 0b00111110,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_S_high[16] = {
    0b00111110, 0b01000000, 0b00111100, 0b00000010,
    0b00111110, 0b00000000, 0b00001100, 0b00000110,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_I_dot_high[16] = {
    0b00011000, 0b00000000, 0b00111100, 0b00011000,
    0b00011000, 0b00011000, 0b00011000, 0b00111100,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_O_high[16] = {
    0b01100110, 0b00000000, 0b00111100, 0b01000010,
    0b01000010, 0b01000010, 0b01000010, 0b00111100,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_C_high[16] = {
    0b00111110, 0b01000000, 0b01000000, 0b01000000,
    0b00111110, 0b00000000, 0b00001100, 0b00000110,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

static uint8_t font_tr_U_high[16] = {
    0b01100110, 0b00000000, 0b01000010, 0b01000010,
    0b01000010, 0b01000010, 0b01000010, 0b00111100,
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000
};

void vga_upload_char(uint8_t ascii, uint8_t *bitmap) {
    // Plane 2'yi (Font) seç
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    outb(0x3C4, 0x04); outb(0x3C5, 0x07);
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);
    outb(0x3CE, 0x06); outb(0x3CF, 0x00);

    uint8_t *vga_mem = (uint8_t *)0xA0000;
    for (int i = 0; i < 16; i++) {
        vga_mem[ascii * 32 + i] = bitmap[i];
    }

    // Metin moduna geri dön
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x04); outb(0x3C5, 0x03);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
}

void vga_load_tr_font(void) {
    // Küçükler (1-6)
    vga_upload_char(1, font_tr_g_low);
    vga_upload_char(2, font_tr_s_low);
    vga_upload_char(3, font_tr_i_dotless);
    vga_upload_char(4, font_tr_o_low);
    vga_upload_char(5, font_tr_c_low); // 'ç' harfi 5 numaralı slota
    vga_upload_char(6, font_tr_u_low);

    // Büyükler (7-12)
    vga_upload_char(7, font_tr_G_high);     // Ğ
    vga_upload_char(8, font_tr_S_high);     // Ş
    vga_upload_char(9, font_tr_I_dot_high); // İ
    vga_upload_char(10, font_tr_O_high);    // Ö
    vga_upload_char(11, font_tr_C_high);    // Ç
    vga_upload_char(12, font_tr_U_high);    // Ü
}