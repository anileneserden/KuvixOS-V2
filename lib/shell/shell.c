#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <kernel/serial.h>  // <--- BU EKSİK OLABİLİR (serial_received ve serial_getc için)
#include <lib/commands.h>
#include <kernel/vga_font.h>
#include <kernel/vga.h>

void shell_readline(char* buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = 0;

        kbd_poll(); 

        if (kbd_has_character()) { 
            c = kbd_get_char();
        } else if (serial_received()) {
            c = serial_getc();
        }

        if (c == 0) continue;

        // 1. ENTER: Satırı bitir
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            vga_putc('\n');
            serial_putc('\r'); // Seri portta yeni satır için \r\n gerekebilir
            serial_putc('\n');
            break;
        } 
        // 2. BACKSPACE: Karakteri sil
        else if (c == '\b' || c == 8 || c == 127) { 
            if (i > 0) {
                i--;
                // VGA ekranından sil
                vga_putc('\b');
                vga_putc(' ');
                vga_putc('\b');
                // Seri porttan (Terminal) sil
                serial_putc('\b');
                serial_putc(' ');
                serial_putc('\b');
            }
        } 
        // 3. YAZILABİLİR KARAKTERLER: Ekrana bas ve kaydet
        else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            vga_putc(c);
            serial_putc(c);
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
