#ifndef PRINTK_H
#define PRINTK_H

#include <stdarg.h>

/**
 * Ekrana (VGA) ve Seri Porta formatlı çıktı verir.
 */
void printk(const char* fmt, ...);

/**
 * Verilen formatlı metni bir buffer'a (string dizisine) yazar.
 * Installer ve UI elemanları için kritiktir.
 */
int ksprintf(char *buf, const char *fmt, ...);

#endif