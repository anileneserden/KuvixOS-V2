// kernel/exec/kef/kef_loader.c
#include <kernel/exec/kef.h>
#include <kernel/exec/kef_api.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>

#include <stdint.h>
#include <stdbool.h>

static int vfs_ok(int rc) { return rc == 1; }

static int kef_read_entire_file(const char* path, uint8_t** out_buf, uint32_t* out_size) {
    if (!path || !out_buf || !out_size) return -1;
    *out_buf = 0;
    *out_size = 0;

    uint32_t cap = 1024 * 1024; // 1MB limit
    uint8_t* buf = (uint8_t*)kmalloc(cap);
    if (!buf) return -2;

    uint32_t sz = 0;
    int rc = vfs_read_all(path, buf, cap, &sz);
    if (!vfs_ok(rc) || sz < sizeof(kef_header_t)) {
        printk("[KEF] read_all failed path=%s rc=%d sz=%u\n", path, rc, (unsigned)sz);
        kfree(buf);
        return -3;
    }

    *out_buf = buf;
    *out_size = sz;
    return 0;
}

static bool kef_header_valid(const kef_header_t* h, uint32_t file_size) {
    if (!h) return false;
    if (file_size < (uint32_t)sizeof(kef_header_t)) return false;
    if (h->magic != KEF_MAGIC) return false;
    if (h->version != 1) return false;

    uint32_t max_image = file_size - (uint32_t)sizeof(kef_header_t);
    if (h->image_size == 0) return false;
    if (h->image_size > max_image) return false;
    if (h->entry_rva >= h->image_size) return false;

    return true;
}

// TODO: senin reloc formatın varsa burada uygula
static int kef_apply_relocs(uint8_t* img, uint32_t img_sz,
                            const uint8_t* file_buf, uint32_t file_sz,
                            const kef_header_t* h) {
    (void)img; (void)img_sz; (void)file_buf; (void)file_sz; (void)h;
    // printk("[KEF] reloc applied count=0\n");
    return 0;
}

/*
  ✅ Yeni protokol ile “tek-shot” çalıştırma:
    int _start(const kvx_api_t* api, kvx_kef_app_t* out_vtbl)

  Bu fonksiyon host modelinde şart değil ama compile’ın düzelmesi için doğru hale getiriyoruz.
*/
int kef_exec(const char* path) {
    if (!path || !path[0]) return -1;

    uint8_t* file_buf = 0;
    uint32_t file_sz  = 0;

    int rc = kef_read_entire_file(path, &file_buf, &file_sz);
    if (rc != 0) {
        printk("[KEF] read failed path=%s rc=%d\n", path, rc);
        return rc;
    }

    const kef_header_t* h = (const kef_header_t*)file_buf;
    if (!kef_header_valid(h, file_sz)) {
        printk("[KEF] invalid header path=%s size=%u\n", path, (unsigned)file_sz);
        kfree(file_buf);
        return -2;
    }

    uint32_t total = h->image_size + h->bss_size;
    uint8_t* img = (uint8_t*)kmalloc(total);
    if (!img) {
        kfree(file_buf);
        return -3;
    }

    const uint8_t* src_image = file_buf + sizeof(kef_header_t);
    memcpy(img, src_image, h->image_size);
    if (h->bss_size) memset(img + h->image_size, 0, h->bss_size);

    // reloc uygula
    kef_apply_relocs(img, h->image_size, file_buf, file_sz, h);

    kfree(file_buf);
    file_buf = 0;

    kvx_kef_entry_fn_t entry = (kvx_kef_entry_fn_t)(uintptr_t)(img + h->entry_rva);

    printk("[KEF] exec %s img=%p entry=%p image=%u bss=%u\n",
           path, img, (void*)entry, (unsigned)h->image_size, (unsigned)h->bss_size);

    // ✅ artık entry out_vtbl dolduruyor
    kvx_kef_app_t vtbl;
    memset(&vtbl, 0, sizeof(vtbl));

    kef_api_set_active(1);
    int erc = entry(&g_kvx_api, &vtbl);
    kef_api_set_active(0);

    printk("[KEF] entry rc=%d (%s)\n", erc, path);

    if (erc != 0) {
        kfree(img);
        return -4;
    }

    // Tek-shot test: create/draw çağırıp çık
    if (vtbl.on_create) {
        kef_api_set_active(1);
        vtbl.on_create(&g_kvx_api);
        kef_api_set_active(0);
    }
    if (vtbl.on_draw) {
        kef_api_set_active(1);
        vtbl.on_draw(&g_kvx_api);
        kef_api_set_active(0);
    }

    kfree(img);
    return 0;
}