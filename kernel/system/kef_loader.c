#include <kernel/kef_v3.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

typedef void (*entry_point_t)(void);

int run_kef_v3(void* buffer) {
    if (!buffer) return -1;

    kef_v3_header_t* header = (kef_v3_header_t*)buffer;

    // 1. İmzayı (Magic) Kontrol Et
    if (header->magic != KEF_V3_MAGIC) {
        printk("[Loader] Hata: Gecersiz KEF imzasi (0x%x)\n", header->magic);
        return -2;
    }

    // 2. Versiyon Kontrolü
    if (header->version != 3) {
        printk("[Loader] Hata: Desteklenmeyen sürüm v%d\n", header->version);
        return -3;
    }

    // 3. Uygulama İçin Bellek Ayır (Kod + Heap)
    // Uygulama motorunun güvenle çalışması için kmalloc kullanıyoruz
    uint32_t total_size = header->text_size + header->heap_size;
    void* exec_mem = kmalloc(total_size);
    
    if (!exec_mem) {
        printk("[Loader] Hata: Bellek yetersiz!\n");
        return -4;
    }

    // Belleği temizle (0 ile doldur)
    memset(exec_mem, 0, total_size);

    // 4. Kod Kısmını Header'ın Hemen Sonrasından Kopyala
    // Header 28 byte'tır, kod buradan sonra başlar
    memcpy(exec_mem, (uint8_t*)buffer + sizeof(kef_v3_header_t), header->text_size);

    printk("[Loader] Uygulama yuklendi: %d byte, Entry: 0x%x\n", 
            header->text_size, header->entry_point);

    // 5. Uygulamaya Zıpla
    // Entry point genellikle 0'dır (kodun başlangıcı), 
    // ama header içinde farklı bir değer varsa onu ekliyoruz.
    entry_point_t entry = (entry_point_t)((uint32_t)exec_mem + header->entry_point);
    
    // DİKKAT: Uygulamayı çağırıyoruz!
    entry();

    // Uygulama dönerse (genelde sonsuz döngüde kalır ama) belleği serbest bırakabilirsin
    // kfree(exec_mem); 
    return 0;
}