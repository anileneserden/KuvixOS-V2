#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <kernel/serial.h>  // <--- BU EKSİK OLABİLİR (serial_received ve serial_getc için)
#include <lib/commands.h>
#include <kernel/vga_font.h>

void shell_readline(char* buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = 0;

        // 1. Önce seri porttan (konsoldan) veri var mı diye bak
        if (serial_received()) { 
            c = serial_getc();
        } 
        // 2. Yoksa klavyeden veri gelmiş mi diye bak
        else if (kbd_has_character()) { 
            c = kbd_get_char();
        }

        if (c == 0) continue; // Hiçbir veri yoksa döngüye devam et

        // Karakter işleme mantığı
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            printk("\n");
            break;
        } 
        else if (c == '\b' || c == 127) { // 127 bazı konsollarda Backspace'dir
            if (i > 0) {
                i--;
                printk("\b \b");
            }
        } 
        else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            printk("%c", c); // Bu hem ekrana hem seri porta basar
        }
    }
}

void shell_init(void) {
    vga_load_tr_font();
    kbd_init();
    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n\n");

    char line[128];
    while (1) {
        // \n\n karakterlerini sildik, böylece imleç promptun hemen yanında bekler.
        printk("KuvixOS> "); 
        
        shell_readline(line, sizeof(line));
        
        if (line[0] != '\0') {
            commands_execute(line);
            // Komut bittikten sonra yeni satıra geçmek iyidir ama 
            // commands_execute zaten bir çıktı veriyorsa buna gerek kalmayabilir.
            // printk("\n"); 
        }
    }
}
