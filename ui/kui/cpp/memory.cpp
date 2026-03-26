#include <stddef.h>
#include <kernel/memory/kmalloc.h>

void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete(void* ptr, size_t) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}

void operator delete[](void* ptr, size_t) noexcept {
    if (ptr) {
        kfree(ptr);
    }
}