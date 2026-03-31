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
    return (r == 1);
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
        "/system/ui/desktop.json",
        "{\n"
        "  \"type\": \"Screen\",\n"
        "  \"backgroundColor\": \"#000000\",\n"
        "  \"children\": [\n"
        "    {\n"
        "      \"type\": \"Window\",\n"
        "      \"id\": \"mainWindow\",\n"
        "      \"x\": 100,\n"
        "      \"y\": 80,\n"
        "      \"width\": 320,\n"
        "      \"height\": 180,\n"
        "      \"title\": \"Kuvix Window\",\n"
        "      \"backgroundColor\": \"#1E1E1E\",\n"
        "      \"titlebarColor\": \"#2A2A2A\",\n"
        "      \"titleColor\": \"#FFFFFF\",\n"
        "      \"titlebarHeight\": 28,\n"
        "      \"radius\": 12\n"
        "    }\n"
        "  ]\n"
        "}\n"
    },
    {
        "/system/ui/window.json",
        "{\n"
        "  \"window\": {\n"
        "    \"x\": 100,\n"
        "    \"y\": 80,\n"
        "    \"width\": 320,\n"
        "    \"height\": 180,\n"
        "    \"title\": \"Kuvix Window\",\n"
        "    \"radius\": 12,\n"
        "    \"borderThickness\": 1,\n"
        "    \"titlebarHeight\": 28,\n"
        "    \"backgroundColor\": \"#1E1E1E\",\n"
        "    \"titlebarColor\": \"#2A2A2A\",\n"
        "    \"titleColor\": \"#FFFFFF\"\n"
        "  }\n"
        "}\n"
    }
};

/* --- entry --- */

void seed_files_run(void) {
    printk("[seed] run\n");

    for (uint32_t i = 0; i < (uint32_t)(sizeof(k_seed_texts) / sizeof(k_seed_texts[0])); i++) {
        write_if_missing(k_seed_texts[i].path, k_seed_texts[i].content);
    }

    printk("[seed] done\n");
}