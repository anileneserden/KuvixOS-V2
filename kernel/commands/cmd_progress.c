#include <kernel/printk.h>
#include <lib/commands.h>
#include <stdint.h>

/**
 * Progress bar çizici. 
 * Satırın başını temizler ve barı günceller.
 */
void show_progress(int current, int total) {
    const int bar_width = 30;
    int progress = (current * 100) / total;
    int pos = (current * bar_width) / total;

    char my_buffer[128];
    int idx = 0;

    // Sadece satır başına dön ve barı hazırla
    my_buffer[idx++] = '\r';
    my_buffer[idx++] = '[';

    for (int i = 0; i < bar_width; i++) {
        if (i < pos) my_buffer[idx++] = '=';
        else if (i == pos) my_buffer[idx++] = '>';
        else my_buffer[idx++] = ' ';
    }

    my_buffer[idx++] = ']';
    my_buffer[idx++] = ' ';

    // Yüzde rakamları
    if (progress == 0) {
        my_buffer[idx++] = '0';
    } else {
        if (progress == 100) {
            my_buffer[idx++] = '1'; my_buffer[idx++] = '0'; my_buffer[idx++] = '0';
        } else {
            if (progress >= 10) my_buffer[idx++] = (progress / 10) + '0';
            my_buffer[idx++] = (progress % 10) + '0';
        }
    }
    my_buffer[idx++] = '%';
    my_buffer[idx] = '\0';

    // Sadece yazdır (Sonuna \n koyma!)
    commands_puts(my_buffer);
}

void show_download_progress(uint32_t current, uint32_t total, uint32_t speed_kb) {
    const int bar_width = 20; // Hız bilgisine yer açmak için barı biraz daralttık
    int progress = (current * 100) / total;
    int pos = (current * bar_width) / total;

    char my_buffer[256];
    int idx = 0;

    my_buffer[idx++] = '\r';
    my_buffer[idx++] = '[';

    // Bar Çizimi
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) my_buffer[idx++] = '=';
        else if (i == pos) my_buffer[idx++] = '>';
        else my_buffer[idx++] = ' ';
    }
    my_buffer[idx++] = ']';
    my_buffer[idx++] = ' ';

    // 1. Bilgi: Yüzde
    // Önceki itoa mantığını kullanarak buraya "progress" değerini ekle
    
    // 2. Bilgi: İndirilen / Toplam (MB cinsinden)
    // (current / 1024 / 1024) ve (total / 1024 / 1024) hesaplamalarını ekle
    
    // 3. Bilgi: Hız
    // "speed_kb" değerini ekle ve sonuna " KB/s" koy
    
    my_buffer[idx] = '\0';
    commands_puts(my_buffer);
}

static void cmd_progress(int argc, char** argv) {
    (void)argv;
    
    // 1. Döngü bittiğinde %100'ü gör
    for (int i = 0; i <= 100; i += 2) {
        show_progress(i, 100);
        for (volatile uint32_t d = 0; d < 80000000; d++); 
    }

    // 2. ✅ KRİTİK TEMİZLİK: İşlem bittiğinde alt satıra geç
    // Bu terminalin overwrite modundan (len=0) çıkmasını sağlar.
    commands_puts("\n"); 
    commands_puts("Islem tamamlandi.\n");
}

REGISTER_COMMAND(progress, cmd_progress, "Progress bar gorselini test eder");