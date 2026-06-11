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
    printk("[DEBUG] Yuklemeye calisilan dosya yolu: %s\n", path);

    vfs_file_t* file_ptr = 0;

    // 1. Dosyayı aç
    int open_status = vfs_open(path, 0, &file_ptr); 
    if (open_status == 0 || file_ptr == 0) {
        printk("[KDF LOADER] Hata: Surucu dosyasi bulunamadi veya acilamadi!\n");
        return -1;
    }

    // 2. Dosya boyutunu al (Senin vfs_file_t içindeki size alanına doğrudan erişiyoruz)
    // NOT: Eğer vfs_file_t içinde bu alanın adı th_size veya rfd_size gibi farklıysa ona göre uyarla.
    uint32_t file_size = vfs_get_size(file_ptr);
    if (file_size < sizeof(KDF_Header)) {
        printk("[KDF LOADER] Hata: Gecersiz veya bozuk KDF dosyasi!\n");
        vfs_close(file_ptr);
        return -2;
    }

    // 3. Hafızada yer aç
    uint8_t* driver_buffer = (uint8_t*)kmalloc(file_size);
    if (!driver_buffer) {
        printk("[KDF LOADER] Hata: Surucu icin RAM tahsis edilemedi!\n");
        vfs_close(file_ptr);
        return -3;
    }

    // 4. Dosyayı oku 
    uint32_t bytes_read = 0;
    vfs_read(file_ptr, driver_buffer, file_size, &bytes_read);
    vfs_close(file_ptr);

    // Güvenlik kontrolü
    if (bytes_read < sizeof(KDF_Header)) {
        printk("[KDF LOADER] Hata: Surucu dosyasi eksik okundu! (%d/%d bayt)\n", bytes_read, file_size);
        kfree(driver_buffer); // kmfree -> kfree yapıldı
        return -4;
    }

    // 5. Magic (Sihirli Bayt) Kontrolü
    KDF_Header* header = (KDF_Header*)driver_buffer;
    if (header->magic != KDF_MAGIC) {
        printk("[KDF LOADER] Hata: Gecersiz KDF_MAGIC (0x%x)\n", header->magic);
        kfree(driver_buffer); // kmfree -> kfree yapıldı
        return -5;
    }

    printk("[KDF] Bulunan Surucu: %s (Surum: 0x%x)\n", header->driver_name, header->driver_version);

    // 6. Relocation ve Giriş Fonksiyonunun Tespiti
    int (*driver_init)(KernelAPI*) = (int (*)(KernelAPI*))(driver_buffer + header->init_offset);

    // 7. Sürücüyü Çalıştır
    int result = driver_init(&kdf_kernel_api);
    if (result != 0) {
        printk("[KDF LOADER] Hata: Surucu baslatma fonksiyonu hata kodu dondu: %d\n", result);
        kfree(driver_buffer); // kmfree -> kfree yapıldı
        return -6;
    }

    printk("[KDF LOADER] %s surucusu sisteme basariyla baglandi!\n", header->driver_name);
    return 0;
}