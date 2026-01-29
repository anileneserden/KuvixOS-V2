#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/vga_font.h>

void shell_readline(char* buf, int max_len) {
    int len = 0;
    while (len < max_len - 1) {
        // Polling'i kernel ana döngüsünde de yapabiliriz ama
        // şimdilik readline içinde donanımı dinleyelim.
        kbd_poll(); 

        char c = kbd_get_char();
        if (c == 0) continue;

        if (c == '\n') {
            printk("\n");
            buf[len] = '\0';
            return;
        } 
        else if (c == '\b') {
            if (len > 0) {
                len--;
                // VGA üzerinde geri silme: geri git, boşluk bas, geri git.
                printk("\b \b"); 
            }
        } 
        else {
            buf[len++] = c;
            printk("%c", c); // Ekranda karakteri göster
        }
    }
    buf[len] = '\0';
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
