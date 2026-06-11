#include <kernel/kdf.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <arch/x86/io.h>
#include <arch/x86/idt.h>

// Sürücüye teslim edilecek canlı kernel fonksiyon adresleri
static KernelAPI kdf_kernel_api = {
    .printk = printk,
    .inb = inb,
    .outb = outb,
    .register_interrupt = idt_register_irq_handler
};

int kdf_load_driver(const char* path) {
    printk("[KDF LOADER] Yukleniyor: %s\n", path);

    // 1. Dosya boyutunu vfs_stat ile güvenli bir şekilde alalım
    vfs_stat_t st;
    printk("[DEBUG] Dosya boyutu (st.size): %d\n", st.size);
    printk("[DEBUG] sizeof(KDF_Header): %d\n", sizeof(KDF_Header));

    if (!vfs_stat(path, &st)) {
        printk("[KDF LOADER] HATA: Dosya stat bilgisi alinamadi (dosya yok mu?)\n");
        return -1;
    }
    
    uint32_t file_size = st.size;
    if (file_size < sizeof(KDF_Header)) {
        printk("[KDF LOADER] HATA: Gecersiz veya bozuk KDF dosyasi (Boyut yetersiz)\n");
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
        printk("[KDF LOADER] HATA: Okuma basarisiz veya eksik veri!\n");
        vfs_close(file_ptr);
        kfree(driver_buffer);
        return -5;
    }
    vfs_close(file_ptr); // İşimiz bitti, kapatıyoruz

    // 5. Magic (Sihirli Bayt) Kontrolü
    KDF_Header* header = (KDF_Header*)driver_buffer;
    if (header->magic != KDF_MAGIC) {
        printk("[KDF LOADER] HATA: Gecersiz KDF_MAGIC (0x%x)\n", header->magic);
        kfree(driver_buffer);
        return -6;
    }

    printk("[KDF] Bulunan Surucu: %s (Surum: 0x%x)\n", header->driver_name, header->driver_version);

    // 6. Giriş Fonksiyonunu (Entry Point) hesapla
    // driver_buffer başlangıç adresidir, init_offset ise giriş fonksiyonunun uzaklığıdır.
    int (*driver_init)(KernelAPI*) = (int (*)(KernelAPI*))(driver_buffer + header->init_offset);

    // 7. Sürücüyü Çalıştır
    // ÖNEMLİ: driver_buffer'ı burada asla free etmiyoruz, çünkü sürücü kodu orada yaşıyor!
    int result = driver_init(&kdf_kernel_api);
    if (result != 0) {
        printk("[KDF LOADER] HATA: Surucu baslatma basarisiz! Hata kodu: %d\n", result);
        kfree(driver_buffer); // Hata durumunda hafızayı temizle
        return -7;
    }

    printk("[KDF LOADER] %s surucusu basariyla baglandi!\n", header->driver_name);
    return 0;
}