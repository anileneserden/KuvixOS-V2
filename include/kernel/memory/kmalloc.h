#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

void* kmalloc(size_t sz);
void* kmalloc_a(size_t sz); // Hizalanmış

#endif