#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>

void cmd_write(int argc, char** argv) {
    if (argc < 3) {
        commands_puts("Kullanim: write <path> <text...>\n");
        commands_puts("Ornek: write /persist/test/a.txt merhaba dunya\n");
        return;
    }

    const char* path = argv[1];

    char buf[1024];
    buf[0] = 0;

    int pos = 0;
    int remaining = (int)sizeof(buf) - 1;

    for (int i = 2; i < argc; i++) {
        const char* part = argv[i];
        int len = (int)strlen(part);

        if (i > 2) {
            if (remaining <= 0) break;
            buf[pos++] = ' ';
            buf[pos] = 0;
            remaining--;
        }

        if (remaining <= 0) break;

        int take = len;
        if (take > remaining) take = remaining;

        memcpy(buf + pos, part, (uint32_t)take);
        pos += take;
        buf[pos] = 0;
        remaining -= take;
    }

    int wrote = vfs_write_all(path, (const uint8_t*)buf, (uint32_t)strlen(buf));
    if (wrote >= 0) {
        printk("Yazildi: %s (%d byte)\n", path, (int)strlen(buf));
    } else {
        printk("Hata: dosya yazilamadi: %s\n", path);
    }
}

REGISTER_COMMAND(write, cmd_write, "Dosyaya metin yazar");