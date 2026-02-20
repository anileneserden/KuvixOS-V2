// include/kernel/memory/kmalloc.h
#pragma once
#include <stddef.h>
#include <stdint.h>

void  kmalloc_init(void* heap_start, size_t heap_size);

void* kmalloc(size_t size);
void  kfree(void* ptr);

// debug istersen
size_t kmalloc_bytes_free(void);