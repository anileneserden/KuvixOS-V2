#ifndef VGA_FONT_H
#define VGA_FONT_H

#include <stdint.h>

/**
 * @brief Belirli bir ASCII değerine özel bir bitmap yükler.
 * @param ascii Yüklenecek karakter kodu (0-255)
 * @param bitmap 16 byte'lık font verisi (8x16 font için)
 */
void vga_upload_char(uint8_t ascii, uint8_t *bitmap);

/**
 * @brief Tüm Türkçe karakterleri (ğ, ş, ı, ö, ç, ü) 
 * ASCII 128-133 arasına yükler.
 */
void vga_load_tr_font(void);

#endif