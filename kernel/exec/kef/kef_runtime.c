#include <kernel/exec/kef.h>
#include <kernel/exec/kef_api.h>

#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>

#include <stdint.h>
#include <stdbool.h>

static int vfs_ok(int rc) { return rc == 1; }

static bool kef_header_valid(const kef_header_t* h, uint32_t file_size) {
    if (!h) return false;
    if (file_size < (uint32_t)sizeof(kef_header_t)) return false;
    if (h->magic != KEF_MAGIC) return false;
    if (h->version != 1) return false;

    uint32_t max_image = file_size - (uint32_t)sizeof(kef_header_t);
    if (h->image_size == 0) return false;
    if (h->image_size > max_image) return false;

    // entry image içinde olmalı
    if (h->entry_rva >= h->image_size) return false;
    return true;
}

/*
  Senin mk_kef.py “relocs” üretiyor ve sen daha önce
  “[KEF] reloc applied count=..” yazdırmıştın.
  Ben burada basit bir “dosya sonundan reloc tablosu” şablonu bırakıyorum:

  ✅ Eğer senin formatın farklıysa: bu fonksiyonu kendi formatına göre doldur.
  Şu an “reloc tablosu yok” ise count=0 döner.
*/
static int kef_apply_relocs_from_file(
    uint8_t* img,
    uint32_t img_sz,
    const uint8_t* file_buf,
    uint32_t file_sz,
    const kef_header_t* h
) {
    if (!h->reserved1) {
        printk("[KEF] reloc applied count=0\n");
        return 0;
    }

    uint32_t reloc_off   = h->reserved0;
    uint32_t reloc_count = h->reserved1;

    uint32_t reloc_table_file_off =
        sizeof(kef_header_t) + reloc_off;

    if (reloc_table_file_off + reloc_count * 4 > file_sz) {
        printk("[KEF] bad reloc table bounds\n");
        return -1;
    }

    const uint32_t* relocs =
        (const uint32_t*)(file_buf + reloc_table_file_off);

    uintptr_t img_base = (uintptr_t)img;

    for (uint32_t i = 0; i < reloc_count; i++) {
        uint32_t rva = relocs[i];

        if (rva + 4 > img_sz) {
            printk("[KEF] bad reloc rva=%u\n", rva);
            return -2;
        }

        uint32_t* patch = (uint32_t*)(img + rva);
        *patch += (uint32_t)img_base;
    }

    printk("[KEF] reloc applied count=%u\n", reloc_count);
    return 0;
}

static int kef_read_entire_file(const char* path, uint8_t** out_buf, uint32_t* out_size) {
    if (!path || !out_buf || !out_size) return -1;
    *out_buf = 0;
    *out_size = 0;

    // stat size bazı backendlerde 0 gelebiliyor -> read_all ile al
    uint32_t cap = 1024 * 1024; // 1MB limit şimdilik
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

/*
  ✅ PUBLIC: KEF yükle + entry çağır + vtbl doldurt
  out_img : image base (kmalloc) -> host destroy’da free edilecek
  out_vtbl: kernel tarafında doldurulmuş vtbl (host içinde saklanacak)
*/
int kef_load_app(const char* path, uint8_t** out_img, kvx_kef_app_t* out_vtbl) {
    if (!path || !out_img || !out_vtbl) return -1;
    *out_img = 0;
    memset(out_vtbl, 0, sizeof(*out_vtbl));

    uint8_t* file_buf = 0;
    uint32_t file_sz  = 0;

    int rc = kef_read_entire_file(path, &file_buf, &file_sz);
    if (rc != 0) {
        printk("[KEF] read failed path=%s rc=%d\n", path, rc);
        return rc;
    }

    const kef_header_t* h = (const kef_header_t*)file_buf;
    printk("[KEF] sizeof(kef_header_t)=%u\n", sizeof(kef_header_t));
    printk("[KEF] magic=%x version=%u image=%u bss=%u reloc_off=%u reloc_count=%u\n",
        h->magic,
        h->version,
        h->image_size,
        h->bss_size,
        h->reserved0,
        h->reserved1);
        
    if (!kef_header_valid(h, file_sz)) {
        printk("[KEF] invalid header path=%s size=%u\n", path, (unsigned)file_sz);
        kfree(file_buf);
        return -2;
    }

    uint32_t total = h->image_size + h->bss_size;
    uint8_t* img = (uint8_t*)kmalloc(total);
    if (!img) {
        printk("[KEF] kmalloc failed total=%u (%s)\n", (unsigned)total, path);
        kfree(file_buf);
        return -3;
    }

    const uint8_t* src_image = file_buf + sizeof(kef_header_t);
    memcpy(img, src_image, h->image_size);
    if (h->bss_size) memset(img + h->image_size, 0, h->bss_size);

    // reloc uygula (file_buf içindeki tabloya göre)
    kef_apply_relocs_from_file(img, h->image_size, file_buf, file_sz, h);

    kfree(file_buf);
    file_buf = 0;

    uintptr_t img_base = (uintptr_t)img;
    uintptr_t img_end  = img_base + (uintptr_t)total;

    uintptr_t entry_ptr = img_base + (uintptr_t)h->entry_rva;
    if (entry_ptr < img_base || entry_ptr >= img_end) {
        printk("[KEF] entry out of range entry_rva=%u img=[%p..%p] (%s)\n",
               (unsigned)h->entry_rva, (void*)img_base, (void*)img_end, path);
        kfree(img);
        return -4;
    }

    kvx_kef_entry_fn_t entry = (kvx_kef_entry_fn_t)(uintptr_t)entry_ptr;

    printk("[KEF] loaded %s img=%p entry=%p image=%u bss=%u\n",
           path, img, (void*)entry, (unsigned)h->image_size, (unsigned)h->bss_size);

    printk("[KEF] calling entry...\n");
    kef_api_set_active(1);
    int erc = entry(&g_kvx_api, out_vtbl);
    kef_api_set_active(0);

    printk("[KEF] entry rc=%d\n", erc);

    if (erc != 0) {
        printk("[KEF] entry failed rc=%d (%s)\n", erc, path);
        kfree(img);
        return -5;
    }

    // ✅ basic sanity: fonksiyon pointerları null değilse image içinde olmalı
    #define CHECK_FN(fn) do { \
        uintptr_t p = (uintptr_t)out_vtbl->fn; \
        if (p != 0 && (p < img_base || p >= img_end)) { \
            printk("[KEF] bad fn ptr " #fn "=%p img=[%p..%p] (%s)\n", \
                   (void*)p, (void*)img_base, (void*)img_end, path); \
            kfree(img); \
            return -6; \
        } \
    } while (0)

    CHECK_FN(on_create);
    CHECK_FN(on_draw);
    CHECK_FN(on_key);
    CHECK_FN(on_mouse);
    CHECK_FN(on_destroy);

    #undef CHECK_FN

    *out_img = img;
    return 0;
}