#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>

#define APPEND_OLD_CAP 4096
#define APPEND_ADD_CAP 1024
#define APPEND_FINAL_CAP 4096

void cmd_append(int argc, char** argv) {
    if (argc < 3) {
        commands_puts("Kullanim: append <path> <text...>\n");
        commands_puts("Ornek: append /test/a.txt merhaba dunya\n");
        return;
    }

    const char* path = argv[1];
    if (!path || !path[0]) {
        commands_puts("Hata: gecersiz yol.\n");
        return;
    }

    /* Eklenecek metni argv[2..] den birlestir */
    char addbuf[APPEND_ADD_CAP];
    addbuf[0] = 0;

    int add_pos = 0;
    int add_rem = (int)sizeof(addbuf) - 1;

    for (int i = 2; i < argc; i++) {
        const char* part = argv[i];
        int len = (int)strlen(part);

        if (i > 2) {
            if (add_rem <= 0) break;
            addbuf[add_pos++] = ' ';
            addbuf[add_pos] = 0;
            add_rem--;
        }

        if (add_rem <= 0) break;

        int take = len;
        if (take > add_rem) take = add_rem;

        memcpy(addbuf + add_pos, part, (uint32_t)take);
        add_pos += take;
        addbuf[add_pos] = 0;
        add_rem -= take;
    }

    /* Eski dosyayi oku; yoksa bos kabul et */
    uint8_t oldbuf[APPEND_OLD_CAP];
    uint32_t oldsz = 0;

    int r = vfs_read_all(path, oldbuf, sizeof(oldbuf) - 1, &oldsz);
    if (r < 0) {
        oldsz = 0;
        oldbuf[0] = 0;
    } else {
        oldbuf[oldsz] = 0;
    }

    /* Final tamponu hazirla */
    char finalbuf[APPEND_FINAL_CAP];
    int pos = 0;

    if (oldsz > 0) {
        int take = (int)oldsz;
        if (take > (int)sizeof(finalbuf) - 1) {
            take = (int)sizeof(finalbuf) - 1;
        }

        memcpy(finalbuf, oldbuf, (uint32_t)take);
        pos = take;
    }

    int add_len = (int)strlen(addbuf);
    int rem = (int)sizeof(finalbuf) - 1 - pos;

    if (rem > 0 && add_len > 0) {
        int take = add_len;
        if (take > rem) take = rem;

        memcpy(finalbuf + pos, addbuf, (uint32_t)take);
        pos += take;
    }

    finalbuf[pos] = 0;

    int wrote = vfs_write_all(path, (const uint8_t*)finalbuf, (uint32_t)pos);
    if (wrote > 0) {
        printk("Eklendi: %s (%d byte)\n", path, pos);
    } else {
        printk("Hata: append basarisiz: %s\n", path);
    }
}

REGISTER_COMMAND(append, cmd_append, "Dosyanin sonuna metin ekler");