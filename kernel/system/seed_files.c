// kernel/system/seed_files.c
#include <kernel/system/seed_files.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/user.h>
#include <lib/string.h>
#include <stdint.h>

/* xxd -i ile üretilen semboller */
extern unsigned char build_hello_kef[];
extern unsigned int build_hello_kef_len;

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

static int write_binary_if_missing(const char* path, const uint8_t* data, uint32_t len) {
    if (!path || !data || !len) return 0;
    if (file_exists(path)) return 1;

    int ok = vfs_write_all(path, data, len);
    if (!ok) {
        printk("[seed] binary write failed: %s\n", path);
        return 0;
    }

    printk("[seed] created binary: %s (%u bytes)\n", path, (unsigned)len);
    return 1;
}

/* --- text seeds --- */

typedef struct {
    const char* path;
    const char* content;
} seed_text_t;

static const seed_text_t k_seed_texts[] = {
    {
        "/apps/default.theme.json",
        "{\n"
        "  \"name\": \"Kuvix Default App Theme\",\n"
        "  \"version\": \"1.0\",\n"
        "  \"defaults\": {\n"
        "    \"label\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#121212\",\n"
        "        \"fontSize\": 14,\n"
        "        \"padding\": 0\n"
        "      }\n"
        "    },\n"
        "    \"button\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#ffffff\",\n"
        "        \"backgroundColor\": \"#2c2c2c\",\n"
        "        \"borderColor\": \"#444444\",\n"
        "        \"borderWidth\": 1,\n"
        "        \"borderRadius\": 6,\n"
        "        \"padding\": 8\n"
        "      },\n"
        "      \"hover\": {\n"
        "        \"backgroundColor\": \"#3a3a3a\"\n"
        "      },\n"
        "      \"active\": {\n"
        "        \"backgroundColor\": \"#1f1f1f\"\n"
        "      }\n"
        "    },\n"
        "    \"input\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#ffffff\",\n"
        "        \"placeholderColor\": \"#8e8e8e\",\n"
        "        \"backgroundColor\": \"#1b1b1b\",\n"
        "        \"borderColor\": \"#3c3c3c\",\n"
        "        \"borderWidth\": 1,\n"
        "        \"borderRadius\": 5,\n"
        "        \"padding\": 6\n"
        "      },\n"
        "      \"focused\": {\n"
        "        \"borderColor\": \"#ff8c2a\"\n"
        "      }\n"
        "    }\n"
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

    /* SDK-V2'den gelen .kef dosyası */
    write_binary_if_missing(
        "/apps/hello.kef",
        (const uint8_t*)build_hello_kef,
        (uint32_t)build_hello_kef_len
    );

    write_binary_if_missing(
        USER_DESKTOP_PATH "/hello.kef",
        (const uint8_t*)build_hello_kef,
        (uint32_t)build_hello_kef_len
    );

    printk("[seed] done\n");
}