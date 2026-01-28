#pragma once
#include <stdint.h>

/*
 * x86 Port I/O
 * - outb/outw/outl : port'a yaz
 * - inb/inw/inl    : port'tan oku
 *
 * Not: "memory" clobber => derleyiciye "memory barrier" etkisi verir (optimizasyon karıştırmasın).
 */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1"
                      :
                      : "a"(val), "Nd"(port)
                      : "memory");
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1"
                      :
                      : "a"(val), "Nd"(port)
                      : "memory");
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1"
                      :
                      : "a"(val), "Nd"(port)
                      : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(ret)
                      : "Nd"(port)
                      : "memory");
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0"
                      : "=a"(ret)
                      : "Nd"(port)
                      : "memory");
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0"
                      : "=a"(ret)
                      : "Nd"(port)
                      : "memory");
    return ret;
}

/* Bazı donanımlar için küçük gecikme gerekebilir */
static inline void io_wait(void) {
    // Eski klasik yöntem: 0x80 portuna dummy write
    __asm__ volatile ("outb %%al, $0x80"
                      :
                      : "a"(0)
                      : "memory");
}
