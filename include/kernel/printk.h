#pragma once

// Değişken sayıda argüman desteği (...)
void printk(const char* fmt, ...);

// Terminal'in yazıları yakalayabilmesi için kanca tipi
typedef void (*printk_hook_t)(const char* str);

// Terminal uygulaması bu fonksiyonu kullanarak kendini kayıt eder
void printk_set_hook(printk_hook_t hook);