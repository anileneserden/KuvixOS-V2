#include "shell.h"
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <commands.h>

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
    kbd_init();
    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n\n");

    char line[128];
    while (1) {
        printk("KuvixOS> ");
        shell_readline(line, sizeof(line));
        
        if (line[0] != '\0') {
            commands_execute(line);
            // KOMUTTAN SONRA SATIR ATLA:
            printk("\n"); 
        }
    }
}