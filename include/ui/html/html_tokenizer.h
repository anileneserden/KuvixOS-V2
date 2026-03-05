#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HTOK_EOF = 0,
    HTOK_TEXT,
    HTOK_TAG_OPEN,
    HTOK_TAG_CLOSE
} html_tok_type_t;

typedef struct {
    html_tok_type_t type;

    const char* start;
    uint32_t len;

    const char* tag;
    uint32_t tag_len;

    const char* href;
    uint32_t href_len;

    bool self_closing;

    const char* id;
    uint16_t id_len;
    
    const char* class_name;
    uint16_t class_len;

    const char* style;
    uint16_t style_len;
} html_tok_t;

typedef struct {
    const char* p;
    const char* end;
} html_lex_t;

void html_lex_init(html_lex_t* lx, const char* data, uint32_t size);
bool html_lex_next(html_lex_t* lx, html_tok_t* out);