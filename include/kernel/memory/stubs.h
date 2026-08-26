// include/kernel/memory/stubs.h
#ifndef STUBS_H
#define STUBS_H

#include <stddef.h>
#include <stdint.h>

// Bellek Yönetimi Köprüsü (kfree -> free eşlemesi)
void free(void* ptr);

// Assert Hata Denetimi Köprüsü
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function);

#endif