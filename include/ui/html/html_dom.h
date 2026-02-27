#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HNODE_ELEMENT = 1,
    HNODE_TEXT = 2
} html_node_type_t;

typedef struct html_node {
    html_node_type_t type;

    const char* tag;
    uint16_t tag_len;

    const char* href;
    uint16_t href_len;

    bool self_closing;

    const char* text;
    uint32_t text_len;

    struct html_node* parent;
    struct html_node* first_child;
    struct html_node* next_sibling;
} html_node_t;

typedef struct {
    html_node_t* root;
} html_doc_t;

html_node_t* html_new_element(const char* tag, uint16_t tag_len);
html_node_t* html_new_text(const char* text, uint32_t len);
void html_append_child(html_node_t* parent, html_node_t* child);
bool html_tag_eq(const html_node_t* n, const char* s);