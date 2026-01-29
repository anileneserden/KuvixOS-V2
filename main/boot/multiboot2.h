// src/multiboot2.h
#pragma once
#include <stdint.h>

#define MB2_BOOTLOADER_MAGIC 0x36D76289

#define MB2_TAG_END          0
#define MB2_TAG_FRAMEBUFFER  8

struct mb2_tag {
    uint32_t type;
    uint32_t size;   // total tag size incl header, 8-aligned
} __attribute__((packed));

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;

    // framebuffer_type == 1 (RGB) için devamı (GRUB çoğu zaman bunu verir)
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
} __attribute__((packed));
