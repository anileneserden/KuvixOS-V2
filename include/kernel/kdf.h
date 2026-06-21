#ifndef KERNEL_KDF_H
#define KERNEL_KDF_H

#include <stdint.h>

#define KDF_MAGIC 0x46444B4B 

// Sürücüye teslim edilecek canlı kernel servisleri
typedef struct {
    void (*printk)(const char* fmt, ...);
    uint8_t (*inb)(uint16_t port);
    void (*outb)(uint16_t port, uint8_t data);
    void (*register_interrupt)(int irq, void (*handler)(void));
} KernelAPI;

// JENERİK SÜRÜCÜ OPERASYONLARI (Her sürücü bunu dolduracak)
typedef struct {
    int (*read)(void* buffer, uint32_t size);
    int (*write)(const void* buffer, uint32_t size);
    int (*control)(const char* command, void* arg, uint32_t arg_size);
} KDF_Operations;

typedef struct {
    uint32_t magic;           // 4
    uint32_t driver_version;  // 4
    char     driver_name[32]; // 32
    uint32_t init_offset;     // 4
    uint32_t exit_offset;     // 4
    uint32_t code_size;       // 4
} __attribute__((packed)) KDF_Header;

// Çekirdeğin hafızasında kayıtlı kalacak sürücü yönetim yapısı
typedef struct {
    char name[32];
    uint32_t version;
    uint8_t* base_address;
    KDF_Operations ops;
    uint8_t active;
} KDF_DriverInstance;

// Dışarıya sunulan çekirdek fonksiyonları
int kdf_load_driver(const char* path);
KDF_DriverInstance* kdf_find_driver(const char* name);
void kdf_list_drivers(void);

#endif