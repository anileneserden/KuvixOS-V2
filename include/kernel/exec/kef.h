#pragma once
#include <stdint.h>

#define KEF_MAGIC0   'K'
#define KEF_MAGIC1   'E'
#define KEF_MAGIC2   'F'
#define KEF_VERSION  2

#define KEF_ARCH_X86_32  1

typedef struct {
    uint8_t  magic[3];       // 'K','E','F'
    uint8_t  version;        // 2
    uint8_t  arch;           // KEF_ARCH_X86_32
    uint8_t  reserved0[3];

    uint32_t preferred_addr; // link zamanindaki taban adres
    uint32_t entry_off;      // preferred_addr'a gore _start offseti

    uint32_t code_size;      // kod+veri blogu boyutu (dosyada yer kaplar)
    uint32_t bss_size;       // dosyada yer kaplamaz, sifirlanir

    uint32_t reloc_count;
    uint32_t reloc_off;      // relocation tablosunun dosya-ici offseti

    char     name[16];
    uint32_t reserved1[4];
} __attribute__((packed)) kef_header_t;

typedef struct {
    uint32_t code_offset;    // kod blogu icinde, duzeltilecek 4 baytin offseti
} __attribute__((packed)) kef_reloc_t;

typedef struct {
    void (*print)(const char* s);
} kef_api_t;

typedef void (*kef_entry_fn)(kef_api_t* api);