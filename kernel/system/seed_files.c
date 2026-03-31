#include <kernel/system/seed_files.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* --- helpers --- */

static void ensure_dir(const char* p) {
    if (!p || !p[0]) return;
    vfs_mkdir(p);
}

static int file_exists(const char* path) {
    uint8_t tmp[1];
    uint32_t out_sz = 0;
    int r;

    if (!path || !path[0]) return 0;

    r = vfs_read_all(path, tmp, sizeof(tmp), &out_sz);
    return (r == 1);
}

static int write_if_missing(const char* path, const char* text) {
    uint32_t len;
    int ok;

    if (!path || !text) return 0;
    if (file_exists(path)) return 1;

    len = (uint32_t)strlen(text);
    ok = vfs_write_all(path, (const uint8_t*)text, len);
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
        "/etc/vhost.conf",
        "deneme.local /home/anil/html/deneme/\n"
        "home.local /home/anil/html/home/\n"
    },
    {
        "/home/anil/html/deneme/index.html",
        "<h1>Deneme Local</h1>\n"
        "<p>KuvixBrowser local vhost test</p>\n"
    },
    {
        "/etc/session.conf",
        "desktop=/system/kui/shells/modern/desktop.json\n"
    },
    {
        "/system/kui/shells/modern/desktop.json",
        "{\n"
        "  \"type\": \"Screen\",\n"
        "  \"backgroundColor\": \"#202020\",\n"
        "  \"children\": [\n"
        "    {\n"
        "      \"id\": \"helloLabel\",\n"
        "      \"type\": \"Label\",\n"
        "      \"x\": 40,\n"
        "      \"y\": 40,\n"
        "      \"text\": \"KuvixOS Shell Runtime\",\n"
        "      \"color\": \"#FFFFFF\"\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"infoLabel\",\n"
        "      \"type\": \"Label\",\n"
        "      \"x\": 40,\n"
        "      \"y\": 70,\n"
        "      \"text\": \"session run ile baslatildi\",\n"
        "      \"color\": \"#AAAAAA\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
    }
};

/* --- entry --- */

void seed_files_run(void) {
    uint32_t i;

    printk("[seed] run\n");

    /* gerekli klasorleri olustur */
    ensure_dir("/etc");

    ensure_dir("/home");
    ensure_dir("/home/anil");
    ensure_dir("/home/anil/html");
    ensure_dir("/home/anil/html/deneme");
    ensure_dir("/home/anil/html/home");

    ensure_dir("/system");
    ensure_dir("/system/kui");
    ensure_dir("/system/kui/shells");
    ensure_dir("/system/kui/shells/modern");

    for (i = 0; i < (uint32_t)(sizeof(k_seed_texts) / sizeof(k_seed_texts[0])); i++) {
        write_if_missing(k_seed_texts[i].path, k_seed_texts[i].content);
    }

    printk("[seed] done\n");
}