#pragma once
#include <stdint.h>
#include <stdbool.h>

#define VHOSTS_MAX        32
#define VHOST_HOST_MAX    64
#define VHOST_PATH_MAX    160
#define VHOST_INDEX_MAX   32

typedef struct {
    char host[VHOST_HOST_MAX];     // "home.local"
    char root[VHOST_PATH_MAX];     // "/home/anil/desktop"
    char index[VHOST_INDEX_MAX];   // "home.html" / "index.html"
    bool autoindex;                // on/off
} vhost_entry_t;

typedef struct {
    vhost_entry_t items[VHOSTS_MAX];
    int count;
} vhosts_table_t;

// Parse /etc/vhosts.conf into table (returns true on success)
bool vhosts_load(const char* path, vhosts_table_t* out);

// Find host in table (returns pointer or NULL)
const vhost_entry_t* vhosts_find(const vhosts_table_t* t, const char* host);