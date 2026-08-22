#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/login_api.h> // Çekirdekteki LoginAPI tanımı için

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#define PT_LOAD 1

static void kernel_log(const char* msg) {
    if (msg) printk("%s", msg);
}

void cmd_kls(int argc, char** argv) {
    const char* filepath = "/sys/login/DefaultLogin.kls";
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        filepath = argv[1];
    }

    printk("[KLS TEST] %s yukleniyor...\n", filepath);

    uint32_t max_size = 64 * 1024;
    uint8_t* file_buf = (uint8_t*)kmalloc(max_size);
    if (!file_buf) {
        printk("[KLS TEST HATA] Bellek ayrilamadi.\n");
        return;
    }

    memset(file_buf, 0, max_size);
    uint32_t nread = 0;
    if (!vfs_read_all(filepath, file_buf, max_size, &nread) || nread == 0) {
        printk("[KLS TEST HATA] Dosya okunamadi!\n");
        kfree(file_buf);
        return;
    }

    printk("[KLS TEST] Okunan bayt: %d\n", nread);

    if (nread < sizeof(elf32_ehdr_t) || 
        file_buf[0] != 0x7F || file_buf[1] != 'E' || 
        file_buf[2] != 'L' || file_buf[3] != 'F') {
        printk("[KLS TEST HATA] Gecersiz ELF formati!\n");
        kfree(file_buf);
        return;
    }

    elf32_ehdr_t* ehdr = (elf32_ehdr_t*)file_buf;
    uint32_t entry_point = ehdr->e_entry;
    printk("[KLS TEST] ELF Dogrulandi! Giris Adresi (e_entry): 0x%x\n", entry_point);

    elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buf + ehdr->e_phoff);
    printk("[KLS TEST] Program Header Sayisi: %d\n", ehdr->e_phnum);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            void* dest = (void*)(uintptr_t)phdr[i].p_vaddr;
            printk("[KLS TEST] Segment %d yukleniyor -> vaddr: 0x%x, filesz: %d, memsz: %d\n", 
                   i, phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);
                   
            if (phdr[i].p_filesz > 0) {
                memcpy(dest, file_buf + phdr[i].p_offset, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((uint8_t*)dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    kfree(file_buf);

    // LoginAPI yapısını dolduralım
    LoginAPI kls_api;
    memset(&kls_api, 0, sizeof(LoginAPI));
    kls_api.screen_width    = fb_get_width();
    kls_api.screen_height   = fb_get_height();
    kls_api.put_pixel       = fb_putpixel;
    kls_api.clear_screen    = fb_clear;
    kls_api.update_display  = fb_present;
    kls_api.log             = kernel_log;

    printk("[KLS TEST] Adrese ve API ile atlaniyor -> 0x%x\n", entry_point);

    // C++ app_main(LoginAPI* api) fonksiyonuna argümanı stack üzerinden verip zıplıyoruz
    __asm__ volatile (
        "push %1\n\t"   // &kls_api pointer'ını stack'e it
        "call *%0\n\t"  // Adrese zıpla
        "add $4, %%esp" // Temizle
        :
        : "r" (entry_point), "r" (&kls_api)
        : "memory"
    );

    printk("[KLS TEST] Atlama sonrasi geri donuldu (Normalde buraya dusmemeli).\n");
}

REGISTER_COMMAND(kls, cmd_kls, "Test KLS Loader");