#ifndef KEF_V3_H
#define KEF_V3_H
#include <stdint.h>

#define KEF_V3_MAGIC 0x5633454B // "KEV3"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_point;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t heap_size;
    uint32_t checksum;
} __attribute__((packed)) kef_v3_header_t;

#endif