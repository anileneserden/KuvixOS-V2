#include <kernel/exec/kef.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>

#include <stdint.h>
#include <stdbool.h>

#define VFS_OK(rc) ((rc) != 0)

static void api_log_impl(const char* s) {
    if (!s) return;
    printk("%s", s);
}

static const kvx_api_t g_kvx_api = {
    .log = api_log_impl
};

static int kef_read_entire_file(const char* path, uint8_t** out_buf, uint32_t* out_size) {
    if (!path || !out_buf || !out_size) return -1;
    *out_buf = 0;
    *out_size = 0;

    // Stat size bug var (RAMFS), o yüzden stat'a güvenme.
    // Direkt read_all ile oku.
    const uint32_t CAP = 64 * 1024; // 64KB (hello.kef için fazlasıyla yeter)
    uint8_t* buf = (uint8_t*)kmalloc(CAP);
    if (!buf) {
        printk("[KEF] kmalloc failed cap=%u (%s)\n", (unsigned)CAP, path);
        return -4;
    }

    uint32_t got = 0;
    int rr = vfs_read_all(path, buf, CAP, &got);
    if (rr == 0) { // senin VFS: OK=1, FAIL=0
        printk("[KEF] vfs_read_all failed rr=%d (%s)\n", rr, path);
        return -2;
    }

    if (got < (uint32_t)sizeof(kef_header_t)) {
        printk("[KEF] too small file got=%u (%s)\n", (unsigned)got, path);
        return -3;
    }

    *out_buf = buf;
    *out_size = got;
    return 0;
}

static bool kef_header_valid(const kef_header_t* h, uint32_t file_size) {
    if (!h) return false;
    if (file_size < (uint32_t)sizeof(kef_header_t)) return false;
    if (h->magic != KEF_MAGIC) return false;
    if (h->version != 1) return false;

    uint32_t max_image = file_size - (uint32_t)sizeof(kef_header_t);
    if (h->image_size > max_image) return false;
    if (h->entry_rva >= h->image_size) return false;

    return true;
}

int kef_exec(const char* path) {
    if (!path || !path[0]) return -1;

    uint8_t* file_buf = 0;
    uint32_t file_sz  = 0;

    int rc = kef_read_entire_file(path, &file_buf, &file_sz);
    if (rc < 0) {
        printk("[KEF] read failed path=%s rc=%d\n", path, rc);
        return rc;
    }

    const kef_header_t* h = (const kef_header_t*)file_buf;
    if (!kef_header_valid(h, file_sz)) {
        printk("[KEF] invalid header path=%s size=%u magic=%08x ver=%u\n",
               path, (unsigned)file_sz, (unsigned)h->magic, (unsigned)h->version);
        return -2;
    }

    uint32_t total = h->image_size + h->bss_size;
    uint8_t* img = (uint8_t*)kmalloc(total);
    if (!img) {
        printk("[KEF] kmalloc failed total=%u (%s)\n", (unsigned)total, path);
        return -3;
    }

    const uint8_t* src_image = file_buf + sizeof(kef_header_t);
    memcpy(img, src_image, h->image_size);
    if (h->bss_size) memset(img + h->image_size, 0, h->bss_size);

    // --- Relocation apply ---
    // reserved0 = reloc_table_off (image içinden offset)
    // reserved1 = reloc_count
    uint32_t reloc_off = h->reserved0;
    uint32_t reloc_cnt = h->reserved1;

    uint32_t need = (uint32_t)sizeof(kef_header_t) + h->image_size + reloc_off + reloc_cnt * 4;
    // Biz reloc_off'u image_size yaptığımız için pratikte table:
    // file_buf + sizeof(header) + h->image_size

    const uint8_t* reloc_base = file_buf + sizeof(kef_header_t) + reloc_off;

    for (uint32_t i = 0; i < reloc_cnt; i++) {
        uint32_t off = *(const uint32_t*)(reloc_base + i * 4);
        if (off + 4 <= h->image_size) {
            uint32_t* p = (uint32_t*)(img + off);
            *p += (uint32_t)(uintptr_t)img;  // base fixup
        }
    }
    printk("[KEF] reloc applied count=%u\n", (unsigned)reloc_cnt);

    kef_entry_fn_t entry = (kef_entry_fn_t)(uintptr_t)(img + h->entry_rva);

    printk("[KEF] exec %s img=%p entry=%p image=%u bss=%u\n",
           path, img, entry, (unsigned)h->image_size, (unsigned)h->bss_size);

    int app_rc = entry(&g_kvx_api);

    printk("[KEF] app returned rc=%d (%s)\n", app_rc, path);
    return app_rc;
}