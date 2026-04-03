#pragma once

#include <stdint.h>

#define KEF_MAGIC_0 'K'
#define KEF_MAGIC_1 'E'
#define KEF_MAGIC_2 'F'
#define KEF_MAGIC_3 '1'

#define KEF_VERSION_1 1

typedef enum {
    KEF_APP_TERMINAL = 1,
    KEF_APP_WINDOW = 2
} kef_app_kind_t;

typedef enum {
    KEF_OP_NOP = 0,
    KEF_OP_PRINT = 1,
    KEF_OP_EXIT = 255
} kef_opcode_t;

typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t version;
    uint16_t app_kind;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t str_offset;
    uint32_t str_size;
} kef_header_t;

typedef struct {
    void (*write)(void* user, const char* s);
    void* user;
} kef_host_t;

int kef_run_buffer(const uint8_t* data, uint32_t size, const kef_host_t* host);
int kef_run_path_terminal(const char* path);
int kef_try_run_command(int argc, char** argv);