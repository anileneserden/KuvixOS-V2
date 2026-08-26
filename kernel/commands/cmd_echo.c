#include <kernel/printk.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/vfs.h> // vfs_get_cwd için gerekli
#include <lib/string.h>

// color.c içindeki global renkleri kullan
extern unsigned int color_get_fg(void);
extern unsigned int color_get_bg(void);

void cmd_echo(int argc, char** argv) {
    fb_console_set_color(color_get_fg(), color_get_bg()); // aktif renk

    int redirect_index = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redirect_index = i;
            break;
        }
    }

    if (redirect_index != -1) {
        if (redirect_index + 1 >= argc) {
            commands_puts("Hata: Hedef dosya belirtilmedi!\n");
            return;
        }

        const char* raw_filepath = argv[redirect_index + 1];

        // Metni birleştir (1. indexten '>' indexine kadar olan kelimeler)
        char content[512];
        int offset = 0;
        content[0] = '\0';

        for (int i = 1; i < redirect_index; i++) {
            char* arg = argv[i];
            int len = strlen(arg);
            
            // Başındaki ve sonundaki tırnak işaretlerini (") atla
            int start_idx = (arg[0] == '"') ? 1 : 0;
            int end_idx = (len > 0 && arg[len - 1] == '"') ? len - 1 : len;

            for (int j = start_idx; j < end_idx; j++) {
                if (offset + 2 < sizeof(content)) {
                    content[offset++] = arg[j];
                }
            }

            // Kelimeler arasına boşluk bırak (son kelime değilse)
            if (i < redirect_index - 1) {
                if (offset + 2 < sizeof(content)) {
                    content[offset++] = ' ';
                }
            }
            content[offset] = '\0';
        }

        // Dosya yolunu çalışma dizinine (CWD) göre çözümle (Path Resolution)
        char final_filepath[256];
        if (raw_filepath[0] != '/') {
            const char* cwd = vfs_get_cwd();
            if (cwd[0] == '/' && cwd[1] == '\0') {
                final_filepath[0] = '/';
                int i = 0;
                while (raw_filepath[i] != '\0' && i < 250) {
                    final_filepath[1 + i] = raw_filepath[i];
                    i++;
                }
                final_filepath[1 + i] = '\0';
            } else {
                int c = 0;
                while (cwd[c] != '\0' && c < 200) {
                    final_filepath[c] = cwd[c];
                    c++;
                }
                if (c > 0 && final_filepath[c - 1] != '/') {
                    final_filepath[c++] = '/';
                }
                int r = 0;
                while (raw_filepath[r] != '\0' && (c + r) < 255) {
                    final_filepath[c + r] = raw_filepath[r];
                    r++;
                }
                final_filepath[c + r] = '\0';
            }
        } else {
            int i = 0;
            while (raw_filepath[i] != '\0' && i < 255) {
                final_filepath[i] = raw_filepath[i];
                i++;
            }
            final_filepath[i] = '\0';
        }

        // Çözümlenen tam yola veriyi yaz
        int res = kvxfs_write_all(final_filepath, (const uint8_t*)content, strlen(content));
        if (!res) {
            commands_puts("Hata: Dosya basariyla yazilamadi / disk hatasi!\n");
        }
        return;
    }

    for (int i = 1; i < argc; i++) {
        commands_printf(argv[i]);
        if (i < argc - 1) commands_puts(" ");
    }
    commands_puts("\n");
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazdirir veya dosyaya kaydeder (ornek: echo mrb > dosya.txt)");