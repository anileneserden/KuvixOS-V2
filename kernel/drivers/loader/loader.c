#include <kernel/drivers/loader/loader.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>
#include <kernel/drivers/pci.h> // PCI fonksiyonlarını dahil ettik
#include <lib/string.h>
#include <lib/elf.h>

static uint8_t loader_buffer[64 * 1024]; 

/**
 * @brief Kayıtlı sürücüleri tutacak basit bir liste yapısı.
 * Gerçek bir OS'te bu bir Linked List olmalıdır.
 */
#define MAX_DRIVERS 32
static driver_t* registered_drivers[MAX_DRIVERS];
static int driver_count = 0;

/**
 * @brief Kernel tarafındaki sürücü kayıt fonksiyonu.
 */
void kernel_register_driver(driver_t* drv) {
    if (!drv || driver_count >= MAX_DRIVERS) return;

    // Sürücüyü listemize ekleyelim
    registered_drivers[driver_count++] = drv;
    printk("[Kernel] Yeni surucu kaydedildi: %s\n", drv->name);
    
    // Sürücünün init fonksiyonunu çağıralım
    if (drv->init) {
        int result = drv->init();
        if (result == 0) {
            printk("[Kernel] %s basariyla init edildi.\n", drv->name);
        } else {
            printk("[Kernel] Hata: %s init basarisiz (%d)\n", drv->name, result);
        }
    }
}

int load_module_from_file(const char* path) {
    uint32_t size = 0;
    
    printk("[Loader] %s okunuyor...\n", path);

    if (!kvxfs_read_all(path, loader_buffer, sizeof(loader_buffer), &size)) {
        printk("[Loader] Hata: %s bulunamadi!\n", path);
        return -1;
    }

    Elf64_Ehdr* hdr = (Elf64_Ehdr*)loader_buffer;
    if (memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0) {
        printk("[Loader] Hata: Gecersiz ELF!\n");
        return -1;
    }

    uint32_t sh_num = hdr->e_shnum;
    uint32_t sh_entsize = hdr->e_shentsize;
    uintptr_t table_base = (uintptr_t)loader_buffer + (uint32_t)hdr->e_shoff;

    for (uint32_t i = 0; i < sh_num; i++) {
        Elf64_Shdr* s = (Elf64_Shdr*)(table_base + (i * sh_entsize));
        
        // Sadece çalıştırılabilir kod içeren (SHT_PROGBITS + SHF_EXECINSTR) bölümleri alalım
        if (s->sh_type == 1 && s->sh_size > 0) { 
            void* module_mem = kmalloc(s->sh_size);
            if (!module_mem) return -1;

            memcpy(module_mem, (void*)((uintptr_t)loader_buffer + (uintptr_t)s->sh_offset), s->sh_size);
            
            // --- API TABLOSUNU DOLDURMA ---
            kernel_api_t api;
            api.printk = printk;
            api.register_driver = kernel_register_driver;
            api.get_pci_count = pci_get_device_count;
            api.get_pci_device = pci_get_device;
            
            // PCI API bağlantıları (pci.c'deki fonksiyonların)
            api.pci_read16 = pci_read16;
            api.pci_write16 = pci_write16;
            api.pci_read32 = pci_read32;

            // Modülü çalıştır
            void (*module_entry)(kernel_api_t*) = (void (*)(kernel_api_t*))module_mem;
            printk("[Loader] Modul giris noktasi: 0x%x\n", (uintptr_t)module_mem);
            module_entry(&api);
        }
    }

    return 0;
}