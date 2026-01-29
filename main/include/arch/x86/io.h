#ifndef IO_H
#define IO_H

#include <stdint.h>

/* 8-bit port yazma */
static inline void outb(uint16_t port, uint8_t val) {
    // %b0: val (al), %w1: port (dx)
    __asm__ volatile ("outb %b0, %w1" 
                      : 
                      : "a"(val), "Nd"(port) 
                      : "memory");
}

/* 8-bit port okuma */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %w1, %b0" 
                      : "=a"(ret) 
                      : "Nd"(port) 
                      : "memory");
    return ret;
}

/* 16-bit port yazma */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %w0, %w1" 
                      : 
                      : "a"(val), "Nd"(port) 
                      : "memory");
}

/* 16-bit port okuma */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %w1, %w0" 
                      : "=a"(ret) 
                      : "Nd"(port) 
                      : "memory");
    return ret;
}

/* 32-bit port yazma */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %w1" 
                      : 
                      : "a"(val), "Nd"(port) 
                      : "memory");
}

/* 32-bit port okuma */
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %w1, %0" 
                      : "=a"(ret) 
                      : "Nd"(port) 
                      : "memory");
    return ret;
}

/* IO Gecikmesi (Eski donanımlar için) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif