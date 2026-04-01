#include <ui/kbi.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>
#include <stddef.h>

/* =========================================================
 * Internal helpers
 * ========================================================= */

static void kbi_zero(kbi_image_t* img) {
    if (!img) return;

    img->width = 0;
    img->height = 0;
    img->palette_size = 0;
    memset(img->palette, 0, sizeof(img->palette));
    img->pixels = 0;
}

bool kbi_is_loaded(const kbi_image_t* img) {
    return (img &&
            img->pixels &&
            img->width > 0 &&
            img->height > 0 &&
            img->palette_size > 0);
}

void kbi_free(kbi_image_t* img) {
    if (!img) return;

    if (img->pixels) {
        kfree(img->pixels);
        img->pixels = 0;
    }

    img->width = 0;
    img->height = 0;
    img->palette_size = 0;
    memset(img->palette, 0, sizeof(img->palette));
}

static int is_space_char(char c) {
    return (c == ' ' || c == '\t' || c == '\r');
}

static char* skip_spaces(char* s) {
    while (*s && is_space_char(*s)) s++;
    return s;
}

static void trim_right(char* s) {
    int n = (int)strlen(s);

    while (n > 0) {
        char c = s[n - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            s[n - 1] = '\0';
            n--;
        } else {
            break;
        }
    }
}

static int starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int parse_decimal(const char* s, int* out) {
    int value = 0;
    int digits = 0;

    if (!s || !*s || !out) return -1;

    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        value = value * 10 + (*s - '0');
        s++;
        digits++;
    }

    if (digits == 0) return -1;

    *out = value;
    return 0;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_hex6(const char* s, uint32_t* out) {
    int value = 0;

    if (!s || !out) return -1;
    if (strlen(s) != 6) return -1;

    for (int i = 0; i < 6; i++) {
        int h = hex_val(s[i]);
        if (h < 0) return -1;
        value = (value << 4) | h;
    }

    *out = (uint32_t)value;
    return 0;
}

static int parse_pixel_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static char* next_line(char** cursor) {
    char* line;
    char* p;

    if (!cursor || !*cursor || !**cursor) return 0;

    line = *cursor;
    p = line;

    while (*p && *p != '\n') p++;

    if (*p == '\n') {
        *p = '\0';
        p++;
    }

    *cursor = p;
    return line;
}

/* =========================================================
 * File reader adapter
 * ========================================================= */

static char* kbi_read_file_text(const char* path) {
    uint8_t* buf = 0;
    uint32_t size = 0;

    if (!path) return 0;

    if (!vfs_read_all_alloc(path, &buf, &size)) {
        printk("kbi: vfs_read_all_alloc failed for %s\n", path);
        return 0;
    }

    if (!buf) {
        printk("kbi: vfs_read_all_alloc returned null for %s\n", path);
        return 0;
    }

    /* vfs_read_all_alloc already NUL-terminates buffer */
    return (char*)buf;
}

/* =========================================================
 * Parser
 *
 * Format:
 * KBI1
 * width=4
 * height=4
 * palette_size=4
 * palette:
 * 0=000000
 * 1=FFFFFF
 * 2=FF0000
 * 3=00FF00
 * pixels:
 * 2233
 * 2233
 * 0011
 * 0011
 * ========================================================= */

int kbi_load(const char* path, kbi_image_t* out) {
    char* text;
    char* cursor;
    char* line;
    int stage = 0;
    int palette_read = 0;
    int pixel_row = 0;

    if (!path || !out) return -1;

    kbi_zero(out);

    text = kbi_read_file_text(path);
    if (!text) {
        printk("kbi: failed to read file: %s\n", path);
        return -2;
    }

    cursor = text;

    while ((line = next_line(&cursor)) != 0) {
        trim_right(line);

        char* s = skip_spaces(line);
        if (*s == '\0') continue;

        /* Stage 0: magic */
        if (stage == 0) {
            if (strcmp(s, "KBI1") != 0) {
                printk("kbi: invalid magic in %s\n", path);
                vfs_free_alloc(text);
                return -3;
            }
            stage = 1;
            continue;
        }

        /* Stage 1: header */
        if (stage == 1) {
            if (starts_with(s, "width=")) {
                if (parse_decimal(s + 6, &out->width) != 0 || out->width <= 0) {
                    printk("kbi: invalid width\n");
                    vfs_free_alloc(text);
                    return -4;
                }
                continue;
            }

            if (starts_with(s, "height=")) {
                if (parse_decimal(s + 7, &out->height) != 0 || out->height <= 0) {
                    printk("kbi: invalid height\n");
                    vfs_free_alloc(text);
                    return -5;
                }
                continue;
            }

            if (starts_with(s, "palette_size=")) {
                if (parse_decimal(s + 13, &out->palette_size) != 0 ||
                    out->palette_size <= 0 ||
                    out->palette_size > 256) {
                    printk("kbi: invalid palette_size\n");
                    vfs_free_alloc(text);
                    return -6;
                }
                continue;
            }

            if (strcmp(s, "palette:") == 0) {
                if (out->width <= 0 || out->height <= 0 || out->palette_size <= 0) {
                    printk("kbi: header incomplete before palette\n");
                    vfs_free_alloc(text);
                    return -7;
                }
                stage = 2;
                continue;
            }

            printk("kbi: unexpected header line: %s\n", s);
            vfs_free_alloc(text);
            return -8;
        }

        /* Stage 2: palette */
        if (stage == 2) {
            if (strcmp(s, "pixels:") == 0) {
                size_t pixel_count;

                if (palette_read != out->palette_size) {
                    printk("kbi: palette count mismatch (%d/%d)\n",
                           palette_read, out->palette_size);
                    vfs_free_alloc(text);
                    return -9;
                }

                pixel_count = (size_t)(out->width * out->height);

                out->pixels = (uint8_t*)kmalloc(pixel_count);
                if (!out->pixels) {
                    printk("kbi: no memory for pixels (%u bytes)\n",
                           (unsigned)pixel_count);
                    vfs_free_alloc(text);
                    return -10;
                }

                memset(out->pixels, 0, pixel_count);

                stage = 3;
                continue;
            }

            {
                char* eq = strchr(s, '=');
                if (!eq) {
                    printk("kbi: invalid palette line\n");
                    vfs_free_alloc(text);
                    return -11;
                }

                *eq = '\0';

                {
                    const char* idx_str = s;
                    const char* hex_str = eq + 1;
                    int idx = 0;
                    uint32_t color = 0;

                    if (parse_decimal(idx_str, &idx) != 0) {
                        printk("kbi: invalid palette index\n");
                        vfs_free_alloc(text);
                        return -12;
                    }

                    if (idx != palette_read) {
                        printk("kbi: palette must be sequential (%d expected, got %d)\n",
                               palette_read, idx);
                        vfs_free_alloc(text);
                        return -13;
                    }

                    if (parse_hex6(hex_str, &color) != 0) {
                        printk("kbi: invalid palette color: %s\n", hex_str);
                        vfs_free_alloc(text);
                        return -14;
                    }

                    out->palette[idx] = color;
                    palette_read++;
                }
            }

            continue;
        }

        /* Stage 3: pixels */
        if (stage == 3) {
            int len;

            if (pixel_row >= out->height) {
                printk("kbi: too many pixel rows\n");
                kbi_free(out);
                vfs_free_alloc(text);
                return -15;
            }

            len = (int)strlen(s);
            if (len != out->width) {
                printk("kbi: row=%d has len=%d expected=%d\n",
                       pixel_row, len, out->width);
                kbi_free(out);
                vfs_free_alloc(text);
                return -18;
            }

            for (int x = 0; x < out->width; x++) {
                int idx = parse_pixel_char(s[x]);

                if (idx < 0 || idx >= out->palette_size) {
                    printk("kbi: invalid pixel char '%c' at row=%d col=%d\n",
                           s[x], pixel_row, x);
                    kbi_free(out);
                    vfs_free_alloc(text);
                    return -16;
                }

                out->pixels[pixel_row * out->width + x] = (uint8_t)idx;
            }

            pixel_row++;
            continue;
        }
    }

    vfs_free_alloc(text);

    if (stage != 3) {
        printk("kbi: file ended before pixels section\n");
        kbi_free(out);
        return -19;
    }

    if (pixel_row != out->height) {
        printk("kbi: pixel row count mismatch (%d/%d)\n",
               pixel_row, out->height);
        kbi_free(out);
        return -20;
    }

    return 0;
}

/* =========================================================
 * Draw
 * ========================================================= */

static void kbi_putpixel(int x, int y, uint32_t color) {
    fb_putpixel(x, y, color);
}

void kbi_draw_scaled(const kbi_image_t* img, int dst_x, int dst_y, int scale) {
    if (!img || !img->pixels || scale <= 0) return;

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            uint8_t idx = img->pixels[y * img->width + x];
            uint32_t color = img->palette[idx];

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    kbi_putpixel(dst_x + x * scale + sx,
                                 dst_y + y * scale + sy,
                                 color);
                }
            }
        }
    }
}