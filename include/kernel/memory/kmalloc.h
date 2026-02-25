// include/kernel/memory/kmalloc.h
#pragma once
#include <stddef.h>
#include <stdint.h>

void  kmalloc_init(void* heap_start, size_t heap_size);

void* kmalloc(size_t size);
void  kfree(void* ptr);

// debug istersen
size_t kmalloc_bytes_free(void);

typedef struct {
    uint32_t used_bytes;
    uint32_t free_bytes;
    uint32_t largest_free;
    uint32_t alloc_count;
    uint32_t free_count;
} kmalloc_stats_t;

void kmalloc_get_stats(kmalloc_stats_t* out);