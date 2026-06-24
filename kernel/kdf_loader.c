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

static void print_padded_string(const char* str, int width) {
    int len = 0;
    while (str[len] && len < width) {
        printk("%c", str[len]);
        len++;
    }
    while (len < width) {
        printk(" ");
        len++;
    }
}

void kdf_list_drivers(void) {
    printk("\n========================================================================\n");
    printk(" ID   SURUCU ADI                 SURUM         TABAN ADRES   DURUM      \n");
    printk("========================================================================\n");
    
    int active_count = 0;

    for (int i = 0; i < g_driver_count; i++) {
        if (g_driver_table[i].active) {
            printk(" [%d]  ", i);
            
            print_padded_string(g_driver_table[i].name, 25);
            
            printk("%x", g_driver_table[i].version);
            
            printk("           ");
            
            printk("%x", (uint32_t)g_driver_table[i].base_address);
            
            printk("     ");
            
            printk("LOADED\n");
            
            active_count++;
        }
    }

    if (active_count == 0) {
        printk(" Sisteme yuklu aktif dinamik surucu bulunmamaktadir.\n");
    }
    
    printk("========================================================================\n\n");
}

// Sistem açılışında belirtilen konfigürasyon dosyasındaki tüm sürücüleri otomatik yükler
void kdf_autoload_drivers(void) {
    const char* config_path = "/sys/config/autoload.cfg";
    printk("[KDF] Otomatik surucu yukleme baslatildi (%s)...\n", config_path);

    // 1. Yapılandırma dosyasını VFS üzerinden aç
    vfs_file_t* file_ptr = 0;
    if (!vfs_open(config_path, VFS_O_RDONLY, &file_ptr)) {
        printk("[KDF] Uyari: Otomatik yukleme dosyasi bulunamadi. Atlaniyor.\n");
        return;
    }

    // 2. Dosya içeriğini okumak için stat ile boyutunu alalım
    vfs_stat_t st;
    if (!vfs_stat(config_path, &st) || st.size == 0) {
        vfs_close(file_ptr);
        return;
    }

    // Geçici bir buffer oluşturup dosyayı tümden okuyalım
    char* buf = (char*)kmalloc(st.size + 1);
    if (!buf) {
        vfs_close(file_ptr);
        return;
    }

    uint32_t bytes_read = 0;
    vfs_read(file_ptr, buf, st.size, &bytes_read);
    buf[bytes_read] = '\0'; // String sonu imi
    vfs_close(file_ptr);

    // 3. Dosyayı satır satır işle (Her satır bir .kdf yolu olacak)
    char* line = buf;
    char* next_line;
    
    while (line && *line) {
        // Bir sonraki satırın başlangıcını bul
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0'; // Mevcut satırın sonunu kes
            next_line++;       // Bir sonraki satıra geçiş yap
        }

        // Satır sonundaki carriage return (\r) varsa temizle (Windows/dos editörleri için)
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        // Boş satırları veya '#' ile başlayan yorum satırlarını atla
        if (strlen(line) > 0 && line[0] != '#') {
            // Canlı canlı mevcut yükleme fonksiyonumuzu çağırıyoruz!
            int res = kdf_load_driver(line);
            if (res != 0) {
                printk("[KDF] Otomatik yukleme hatasi: %s (Hata Kodu: %d)\n", line, res);
            }
        }

        line = next_line;
    }

    kfree(buf);
    printk("[KDF] Otomatik surucu yukleme islemi tamamlandi.\n");
}