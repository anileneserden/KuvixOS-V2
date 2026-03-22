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

    /* ---------------- hello.json ---------------- */
    {
        "/apps/hello.json",
        "{\n"
        "  \"window\": {\n"
        "    \"title\": \"Deneme\",\n"
        "    \"width\": 300,\n"
        "    \"height\": 300,\n"
        "    \"backgroundColor\": \"#121212\"\n"
        "  },\n"
        "  \"widgets\": [\n"
        "    {\n"
        "      \"id\": \"titleLabel\",\n"
        "      \"type\": \"label\",\n"
        "      \"text\": \"Merhaba KuvixOS\",\n"
        "      \"x\": 12,\n"
        "      \"y\": 12,\n"
        "      \"color\": \"#ffffff\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
    },

    /* ---------------- default.theme.json ---------------- */
    {
        "/apps/default.theme.json",
        "{\n"
        "  \"name\": \"Kuvix Default App Theme\",\n"
        "  \"version\": \"1.0\",\n"
        "\n"
        "  \"defaults\": {\n"
        "    \"label\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#121212\",\n"
        "        \"fontSize\": 14,\n"
        "        \"padding\": 0\n"
        "      }\n"
        "    },\n"
        "\n"
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
        "\n"
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
        "  },\n"
        "\n"
        "  \"classes\": {\n"
        "    \"title\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#ffffff\",\n"
        "        \"fontSize\": 18\n"
        "      }\n"
        "    },\n"
        "\n"
        "    \"primaryButton\": {\n"
        "      \"base\": {\n"
        "        \"textColor\": \"#ffffff\",\n"
        "        \"backgroundColor\": \"#ff8c2a\",\n"
        "        \"borderColor\": \"#ff8c2a\",\n"
        "        \"borderRadius\": 8,\n"
        "        \"padding\": 10\n"
        "      },\n"
        "      \"hover\": {\n"
        "        \"backgroundColor\": \"#ff9d47\"\n"
        "      },\n"
        "      \"active\": {\n"
        "        \"backgroundColor\": \"#e67612\"\n"
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

    printk("[seed] done\n");
}