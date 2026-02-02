#ifndef PANIC_H
#define PANIC_H

/**
 * Sistemi geri donulemez bir hata durumunda durdurur.
 * @param message Hata açıklaması
 */
void panic(const char *message);

#endif