#pragma once

// Değişken sayıda argüman desteği (...) eklendi
void printk(const char* fmt, ...);
int ksprintf(char *buf, const char *fmt, ...);