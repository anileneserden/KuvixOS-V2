#include <kernel/kdf.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <arch/x86/io.h>
#include <arch/x86/idt.h>
#include <lib/string.h> // strcmp ve strcpy için

#define MAX_DYNAMIC_DRIVERS 16

// Çekirdekte yüklü aktif sürücülerin listesi
static KDF_DriverInstance g_driver_table[MAX_DYNAMIC_DRIVERS];
static int g_driver_count = 0;

// Sürücüye teslim edilecek canlı kernel fonksiyon adresleri
static KernelAPI kdf_kernel_api = {
    .printk = printk,
    .inb = inb,
    .outb = outb,
    .register_interrupt = idt_register_irq_handler
};

// İsme göre jenerik sürücü bulma fonksiyonu (DEDK'nın kullanacağı altyapı)
KDF_DriverInstance* kdf_find_driver(const char* name) {
    for (int i = 0; i < g_driver_count; i++) {
        if (g_driver_table[i].active && strcmp(g_driver_table[i].name, name) == 0) {
            return &g_driver_table[i];
        }
    }
    return 0; // Bulunamadı
}

int kdf_load_driver(const char* path) {
    printk("[KDF LOADER] Yukleniyor: %s\n", path);

    if (g_driver_count >= MAX_DYNAMIC_DRIVERS) {
        printk("[KDF LOADER] HATA: Maksimum surucu sayisina ulasildi!\n");
        return -8;
    }

    // 1. Dosya boyutunu vfs_stat ile güvenli bir şekilde alalım
    vfs_stat_t st;
    if (!vfs_stat(path, &st)) {
        printk("[KDF LOADER] HATA: Dosya stat bilgisi alinamadi\n");
        return -1;
    }
    
    uint32_t file_size = st.size;
    if (file_size < sizeof(KDF_Header)) {
        printk("[KDF LOADER] HATA: Gecersiz veya bozuk KDF dosyasi\n");
        return -2;
    }

    // 2. Dosyayı VFS üzerinden aç
    vfs_file_t* file_ptr = 0;
    if (!vfs_open(path, VFS_O_RDONLY, &file_ptr)) {
        printk("[KDF LOADER] HATA: Dosya acilamadi!\n");
        return -3;
    }

    // 3. Hafızada yer aç
    uint8_t* driver_buffer = (uint8_t*)kmalloc(file_size);
    if (!driver_buffer) {
        printk("[KDF LOADER] HATA: RAM tahsis edilemedi!\n");
        vfs_close(file_ptr);
        return -4;
    }

    // 4. Dosyayı oku 
    uint32_t bytes_read = 0;
    if (!vfs_read(file_ptr, driver_buffer, file_size, &bytes_read) || bytes_read < sizeof(KDF_Header)) {
        printk("[KDF LOADER] HATA: Okuma basarisiz!\n");
        vfs_close(file_ptr);
        kfree(driver_buffer);
        return -5;
    }
    vfs_close(file_ptr);

    // 5. Magic Kontrolü
    KDF_Header* header = (KDF_Header*)driver_buffer;
    if (header->magic != KDF_MAGIC) {
        printk("[KDF LOADER] HATA: Gecersiz KDF_MAGIC (0x%x)\n", header->magic);
        kfree(driver_buffer);
        return -6;
    }

    // ÖNEMLİ DEĞİŞİKLİK: Sürücü init fonksiyonu artık geriye tescil edeceği ops fonksiyon kancalarını dönecek!
    int (*driver_init)(KernelAPI*, KDF_Operations*) = (int (*)(KernelAPI*, KDF_Operations*))(driver_buffer + header->init_offset);

    // Sürücünün doldurması için boş bir operasyon nesnesi oluşturup adresi veriyoruz
    KDF_Operations local_ops = {0, 0, 0};
    int result = driver_init(&kdf_kernel_api, &local_ops);
    
    if (result != 0) {
        printk("[KDF LOADER] HATA: Surucu baslatma basarisiz! Hata: %d\n", result);
        kfree(driver_buffer);
        return -7;
    }

    // Sürücüyü küresel tablomuza jenerik olarak kaydediyoruz
    KDF_DriverInstance* instance = &g_driver_table[g_driver_count];
    strcpy(instance->name, header->driver_name);
    instance->version = header->driver_version;
    instance->base_address = driver_buffer;
    instance->ops = local_ops; // Sürücünün doldurduğu jenerik read/write/control kancaları buraya kopyalandı!
    instance->active = 1;

    g_driver_count++;

    printk("[KDF LOADER] %s surucusu jenerik modda basariyla sisteme eklendi!\n", header->driver_name);
    return 0;
}

// Sürücü tablosundaki yüklü sürücüleri terminale basar
void kdf_list_drivers(void) {
    printk("--- YUKLENMIS KDF SURUCULERI ---\n");
    if (g_driver_count == 0) {
        printk("Sisteme yuklu aktif dinamik surucu bulunmamaktadir.\n");
        return;
    }

    printk("ID   SURUCU ADI                      SURUM\n");
    printk("--------------------------------------------\n");
    for (int i = 0; i < g_driver_count; i++) {
        if (g_driver_table[i].active) {
            printk("[%d]  %-30s 0x%x\n", i, g_driver_table[i].name, g_driver_table[i].version);
        }
    }
}