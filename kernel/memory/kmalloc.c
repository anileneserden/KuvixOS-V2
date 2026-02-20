// kernel/memory/kmalloc.c
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

typedef struct block_hdr {
    uint32_t size;              // payload size
    uint8_t  free;              // 1 free, 0 used
    uint8_t  _pad[3];
    struct block_hdr* next;
    struct block_hdr* prev;
} block_hdr_t;

static block_hdr_t* g_head = 0;
static void*        g_heap_start = 0;
static size_t       g_heap_size  = 0;

static inline size_t align16(size_t x) { return (x + 15) & ~((size_t)15); }
static inline void*  hdr_to_payload(block_hdr_t* h) { return (void*)((uint8_t*)h + sizeof(block_hdr_t)); }
static inline block_hdr_t* payload_to_hdr(void* p) { return (block_hdr_t*)((uint8_t*)p - sizeof(block_hdr_t)); }

static void split_block(block_hdr_t* b, size_t want) {
    // want already aligned
    if (!b || !b->free) return;

    // remaining bytes after taking want
    // need room for new header + at least 16 bytes payload to be worth splitting
    if (b->size <= want + sizeof(block_hdr_t) + 16) return;

    uint8_t* base = (uint8_t*)hdr_to_payload(b);
    block_hdr_t* nb = (block_hdr_t*)(base + want);

    nb->size = (uint32_t)(b->size - want - sizeof(block_hdr_t));
    nb->free = 1;
    nb->next = b->next;
    nb->prev = b;

    if (b->next) b->next->prev = nb;
    b->next = nb;

    b->size = (uint32_t)want;
}

static void coalesce(block_hdr_t* b) {
    if (!b) return;

    // merge with next
    if (b->next && b->next->free) {
        block_hdr_t* n = b->next;
        b->size = (uint32_t)(b->size + sizeof(block_hdr_t) + n->size);
        b->next = n->next;
        if (b->next) b->next->prev = b;
    }

    // merge with prev
    if (b->prev && b->prev->free) {
        block_hdr_t* p = b->prev;
        p->size = (uint32_t)(p->size + sizeof(block_hdr_t) + b->size);
        p->next = b->next;
        if (p->next) p->next->prev = p;
    }
}

void kmalloc_init(void* heap_start, size_t heap_size) {
    g_heap_start = heap_start;
    g_heap_size  = heap_size;

    g_head = (block_hdr_t*)heap_start;
    g_head->size = (uint32_t)(heap_size - sizeof(block_hdr_t));
    g_head->free = 1;
    g_head->next = 0;
    g_head->prev = 0;

    // İstersen debug:
    // printk("[KMALLOC] init heap=%p size=%u\n", heap_start, (unsigned)heap_size);
}

void* kmalloc(size_t size) {
    if (!g_head || size == 0) return 0;

    size_t want = align16(size);

    // first-fit
    block_hdr_t* b = g_head;
    while (b) {
        if (b->free && b->size >= want) {
            split_block(b, want);
            b->free = 0;

            void* p = hdr_to_payload(b);
            // istersen güvenlik: memset(p,0,want);
            return p;
        }
        b = b->next;
    }

    // heap full
    // printk("[KMALLOC] OOM size=%u\n", (unsigned)want);
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_hdr_t* b = payload_to_hdr(ptr);

    // çok basit sanity: heap dışında ise yut
    if ((uint8_t*)b < (uint8_t*)g_heap_start) return;
    if ((uint8_t*)b >= (uint8_t*)g_heap_start + g_heap_size) return;

    b->free = 1;

    // optional: payload'ı temizlemek istersen
    // memset(ptr, 0xCC, b->size);

    coalesce(b);
}

size_t kmalloc_bytes_free(void) {
    size_t sum = 0;
    for (block_hdr_t* b = g_head; b; b = b->next) {
        if (b->free) sum += b->size;
    }
    return sum;
}