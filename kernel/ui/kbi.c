#include <ui/kbi.h>

#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

#include <kernel/drivers/video/gfx.h> // ✅ gfx_putpixel_alpha, gfx_putpixel

#define KBI_MAGIC 0x3149424B /* 'KBI1' little-endian */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;     // 'KBI1'
    uint16_t w;
    uint16_t h;
    uint32_t flags;     // bit0=1 => ARGB8888
    uint32_t reserved;  // 0
} kbi_hdr_t;
#pragma pack(pop)

bool kbi_load(const char* path, kbi_image_t* out) {
    if (!out) return false;
    out->w = 0;
    out->h = 0;
    out->pixels = 0;

    if (!path || !path[0]) return false;

    // 1) Dosya boyunu öğren
    vfs_stat_t st;
    if (!vfs_stat(path, &st)) return false;
    if (st.type != VFS_T_FILE) return false;

    uint32_t cap = st.size;
    if (cap < sizeof(kbi_hdr_t)) return false;

    // 2) Dosyayı RAM'e oku
    uint8_t* data = (uint8_t*)kmalloc(cap);
    if (!data) return false;

    uint32_t sz = 0;
    if (!vfs_read_all(path, data, cap, &sz) || sz < sizeof(kbi_hdr_t)) {
        kfree(data);
        return false;
    }

    // 3) Header parse
    kbi_hdr_t hdr;
    memcpy(&hdr, data, sizeof(hdr));

    if (hdr.magic != KBI_MAGIC || hdr.w == 0 || hdr.h == 0 || ((hdr.flags & 1u) == 0)) {
        kfree(data);
        return false;
    }

    uint32_t pix_count = (uint32_t)hdr.w * (uint32_t)hdr.h;
    uint32_t need = (uint32_t)sizeof(kbi_hdr_t) + pix_count * 4u;
    if (sz < need) {
        kfree(data);
        return false;
    }

    // 4) Pixel buffer kopyala (kalıcı)
    uint32_t* pix = (uint32_t*)kmalloc(pix_count * 4u);
    if (!pix) {
        kfree(data);
        return false;
    }

    memcpy(pix, data + sizeof(kbi_hdr_t), pix_count * 4u);
    kfree(data);

    out->w = hdr.w;
    out->h = hdr.h;
    out->pixels = pix;
    return true;
}

void kbi_free(kbi_image_t* img) {
    if (!img) return;
    if (img->pixels) {
        kfree(img->pixels);
        img->pixels = 0;
    }
    img->w = 0;
    img->h = 0;
}

void kbi_draw(const kbi_image_t* img, int x, int y) {
    if (!img || !img->pixels || img->w == 0 || img->h == 0) return;

    for (int iy = 0; iy < (int)img->h; iy++) {
        for (int ix = 0; ix < (int)img->w; ix++) {
            uint32_t p = img->pixels[iy * (int)img->w + ix];

            uint8_t a = (uint8_t)((p >> 24) & 0xFF);
            if (a == 0) continue;

            uint8_t r = (uint8_t)((p >> 16) & 0xFF);
            uint8_t g = (uint8_t)((p >> 8)  & 0xFF);
            uint8_t b = (uint8_t)(p & 0xFF);

            if (a == 255) {
                // ✅ hızlı yol (origin de gfx_putpixel içinde)
                gfx_putpixel(x + ix, y + iy, (r << 16) | (g << 8) | b);
            } else {
                // ✅ mevcut gfx alpha blender (origin + fb_getpixel okuma dahil)
                gfx_putpixel_alpha(x + ix, y + iy, r, g, b, a);
            }
        }
    }
}