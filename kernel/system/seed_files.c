#include <kernel/system/seed_files.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* --- helpers --- */

static int file_exists(const char* path) {
    uint8_t tmp[1];
    uint32_t out_sz = 0;
    int r = vfs_read_all(path, tmp, 1, &out_sz);
    return (r == 1); // read_all başarılıysa var say
}

static int write_if_missing(const char* path, const char* text) {
    if (!path || !text) return 0;
    if (file_exists(path)) return 1;

    uint32_t len = (uint32_t)strlen(text);
    int ok = vfs_write_all(path, (const uint8_t*)text, len);
    if (!ok) {
        printk("[seed] write failed: %s\n", path);
        return 0;
    }

    printk("[seed] created: %s (%u bytes)\n", path, (unsigned)len);
    return 1;
}

/* --- seeds --- */

typedef struct {
    const char* path;
    const char* content;
} seed_text_t;

static const seed_text_t k_seed_texts[] = {
    {
        "/system/tui/main.cfg",
        "title=KuvixOS\n"
        "item=Terminal|session:tty1\n"
        "item=Desktop|session:desktop\n"
        "item=Reboot|sys:reboot\n"
        "item=Power Off|sys:poweroff\n"
        "item=Languages|cfg:/system/tui/languages.cfg\n"
    },
    {
        "/system/tui/languages.cfg",
        "title=Keyboard Layout\n"
        "item=Turkish Q|cmd:layout trq\n"
        "item=English US|cmd:layout us\n"
        "item=Back|cfg:/system/tui/main.cfg\n"
    },
    {
        "/home/readme.txt",
        "Welcome to KuvixOS!\n"
        "Type 'help' in terminal.\n"
    }
};

/* --- entry --- */

void seed_files_run(void) {
    printk("[seed] run\n");

    // Not: mkdir API'n yoksa sorun değil.
    // vfs_write_all path'e göre entry açıyor (kvxfs/ramfs). Klasör mantığı yoksa da çalışır.
    // İleride gerçek dir olursa burada ensure_dir eklersin.

    for (uint32_t i = 0; i < (uint32_t)(sizeof(k_seed_texts) / sizeof(k_seed_texts[0])); i++) {
        write_if_missing(k_seed_texts[i].path, k_seed_texts[i].content);
    }

    printk("[seed] done\n");
}