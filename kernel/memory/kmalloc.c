// kernel/memory/kmalloc.c
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>

// ------------------------------------------------------------
// Debug guards
// ------------------------------------------------------------
#define KM_MAGIC 0xC0FFEEAAu

typedef struct block_hdr {
    uint32_t magic;              // ✅ corruption guard
    uint32_t size;               // payload size (bytes)
    uint8_t  free;               // 1 free, 0 used
    uint8_t  _pad[3];
    struct block_hdr* next;
    struct block_hdr* prev;
} block_hdr_t;

static block_hdr_t* g_head        = 0;
static void*        g_heap_start  = 0;
static size_t       g_heap_size   = 0;
static uint32_t     g_alloc_count = 0;
static uint32_t     g_free_count  = 0;

static inline size_t align16(size_t x) { return (x + 15) & ~((size_t)15); }
static inline void*  hdr_to_payload(block_hdr_t* h) { return (void*)((uint8_t*)h + sizeof(block_hdr_t)); }
static inline block_hdr_t* payload_to_hdr(void* p) { return (block_hdr_t*)((uint8_t*)p - sizeof(block_hdr_t)); }

static inline int in_heap_ptr(const void* p) {
    if (!g_heap_start || g_heap_size == 0) return 0;
    const uint8_t* b = (const uint8_t*)p;
    const uint8_t* s = (const uint8_t*)g_heap_start;
    const uint8_t* e = s + g_heap_size;
    return (b >= s && b < e);
}

static inline int hdr_sane(block_hdr_t* b) {
    if (!b) return 0;
    if (!in_heap_ptr(b)) return 0;
    if (b->magic != KM_MAGIC) return 0;

    // next/prev pointer sanity (0 olabilir)
    if (b->next && !in_heap_ptr(b->next)) return 0;
    if (b->prev && !in_heap_ptr(b->prev)) return 0;

    // size sanity: payload, heap sınırlarını aşmasın
    // (kabaca: header + payload heap içinde kalmalı)
    uint8_t* start = (uint8_t*)b;
    uint8_t* end   = start + sizeof(block_hdr_t) + (size_t)b->size;
    uint8_t* heap_end = (uint8_t*)g_heap_start + g_heap_size;
    if (end < start) return 0;               // overflow
    if (end > heap_end) return 0;            // heap dışına taşıyor
    return 1;
}

// ------------------------------------------------------------
// split / coalesce
// ------------------------------------------------------------
static void split_block(block_hdr_t* b, size_t want) {
    // want already aligned
    if (!b || !b->free) return;
    if (!hdr_sane(b)) {
        printk("[KMALLOC] CORRUPTION in split_block b=%p\n", b);
        return;
    }

    // remaining bytes after taking want
    // need room for new header + at least 16 bytes payload to be worth splitting
    if (b->size <= want + sizeof(block_hdr_t) + 16) return;

    uint8_t* base = (uint8_t*)hdr_to_payload(b);
    block_hdr_t* nb = (block_hdr_t*)(base + want);

    // nb must sit inside heap
    if (!in_heap_ptr(nb)) {
        printk("[KMALLOC] CORRUPTION split nb out of heap nb=%p\n", nb);
        return;
    }

    nb->magic = KM_MAGIC;
    nb->size  = (uint32_t)(b->size - want - sizeof(block_hdr_t));
    nb->free  = 1;
    nb->next  = b->next;
    nb->prev  = b;

    if (b->next) b->next->prev = nb;
    b->next = nb;

    b->size = (uint32_t)want;

    // post-check
    if (!hdr_sane(b) || !hdr_sane(nb)) {
        printk("[KMALLOC] CORRUPTION after split b=%p nb=%p\n", b, nb);
    }
}

static void coalesce(block_hdr_t* b) {
    if (!b) return;
    if (!hdr_sane(b)) {
        printk("[KMALLOC] CORRUPTION in coalesce b=%p\n", b);
        return;
    }

    // önce prev ile birleş (b'nin başı geriye kayabilir)
    if (b->prev && b->prev->free) {
        block_hdr_t* p = b->prev;
        if (!hdr_sane(p)) {
            printk("[KMALLOC] CORRUPTION prev in coalesce p=%p\n", p);
            return;
        }

        p->size = (uint32_t)(p->size + sizeof(block_hdr_t) + b->size);
        p->next = b->next;
        if (p->next) p->next->prev = p;
        b = p; // ✅ artık birleşik blok p

        if (!hdr_sane(b)) {
            printk("[KMALLOC] CORRUPTION after prev-merge b=%p\n", b);
            return;
        }
    }

    // sonra next ile birleş (artık b doğru blok)
    if (b->next && b->next->free) {
        block_hdr_t* n = b->next;
        if (!hdr_sane(n)) {
            printk("[KMALLOC] CORRUPTION next in coalesce n=%p\n", n);
            return;
        }

        b->size = (uint32_t)(b->size + sizeof(block_hdr_t) + n->size);
        b->next = n->next;
        if (b->next) b->next->prev = b;

        if (!hdr_sane(b)) {
            printk("[KMALLOC] CORRUPTION after next-merge b=%p\n", b);
            return;
        }
    }
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------
void kmalloc_init(void* heap_start, size_t heap_size) {
    g_heap_start  = heap_start;
    g_heap_size   = heap_size;
    g_alloc_count = 0;
    g_free_count  = 0;

    g_head = (block_hdr_t*)heap_start;

    g_head->magic = KM_MAGIC;
    g_head->size  = (uint32_t)(heap_size - sizeof(block_hdr_t));
    g_head->free  = 1;
    g_head->next  = 0;
    g_head->prev  = 0;

    printk("[KMALLOC] init heap=%p size=%u head=%p head_size=%u hdr=%u\n",
           heap_start,
           (unsigned)heap_size,
           g_head,
           (unsigned)g_head->size,
           (unsigned)sizeof(block_hdr_t));
}

void* kmalloc(size_t size) {
    if (!g_head || size == 0) return 0;

    size_t want = align16(size);

    // first-fit
    block_hdr_t* b = g_head;
    while (b) {
        if (!hdr_sane(b)) {
            printk("[KMALLOC] CORRUPTION walk b=%p\n", b);
            return 0;
        }

        if (b->free && b->size >= want) {
            split_block(b, want);
            b->free = 0;
            g_alloc_count++;

            void* p = hdr_to_payload(b);

            // ✅ BU LOGU EKLE:
            printk("[KM_DBG] ALLOC: %u bytes (aligned %u) at %p\n", 
                (unsigned)size, (unsigned)want, p);

            return p;
        }

        b = b->next;
    }

    kmalloc_stats_t s;
    kmalloc_get_stats(&s);
    printk("[KMALLOC] OOM want=%u used=%u free=%u largest=%u alloc=%u freec=%u\n",
        (unsigned)want,
        (unsigned)s.used_bytes,
        (unsigned)s.free_bytes,
        (unsigned)s.largest_free,
        (unsigned)s.alloc_count,
        (unsigned)s.free_count);

    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;

    // ✅ BU LOGU EKLE:
    printk("[KM_DBG] FREE: %p\n", ptr);

    block_hdr_t* b = payload_to_hdr(ptr);

    // heap dışında ise yut
    if (!in_heap_ptr(b)) return;

    // corruption guard
    if (b->magic != KM_MAGIC) {
        printk("[KMALLOC] CORRUPTION free ptr=%p hdr=%p magic=%x\n",
               ptr, b, (unsigned)b->magic);
        return;
    }

    if (b->free) return;
    b->free = 1;
    g_free_count++;

    coalesce(b);
}

size_t kmalloc_bytes_free(void) {
    size_t sum = 0;
    for (block_hdr_t* b = g_head; b; b = b->next) {
        if (!hdr_sane(b)) {
            printk("[KMALLOC] CORRUPTION in bytes_free b=%p\n", b);
            break;
        }
        if (b->free) sum += b->size;
    }
    return sum;
}

void kmalloc_get_stats(kmalloc_stats_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    out->alloc_count = g_alloc_count;
    out->free_count  = g_free_count;

    uint32_t used = 0;
    uint32_t fre  = 0;
    uint32_t largest = 0;

    for (block_hdr_t* b = g_head; b; b = b->next) {
        if (!hdr_sane(b)) {
            printk("[KMALLOC] CORRUPTION in stats walk b=%p\n", b);
            break;
        }

        if (b->free) {
            fre += b->size;
            if (b->size > largest) largest = b->size;
        } else {
            used += b->size;
        }
    }

    out->used_bytes   = used;
    out->free_bytes   = fre;
    out->largest_free = largest;
}