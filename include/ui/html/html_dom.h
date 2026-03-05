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

    const char* id;
    uint16_t id_len;

    const char* class_name;
    uint16_t class_len;

    const char* style;
    uint16_t style_len;

    // computed style
    uint32_t css_bg, css_fg;
    bool css_has_bg, css_has_fg;
} html_node_t;

typedef struct {
    html_node_t* root;

    html_node_t* body;
    bool has_body;

    const char* title;
    uint16_t title_len;
} html_doc_t;

html_node_t* html_new_element(const char* tag, uint16_t tag_len);
html_node_t* html_new_text(const char* text, uint32_t len);
void html_append_child(html_node_t* parent, html_node_t* child);
bool html_tag_eq(const html_node_t* n, const char* s);