#include <ui/html/html_dom.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>

static char to_lower(char c){
    if (c>='A' && c<='Z') return (char)(c - 'A' + 'a');
    return c;
}

html_node_t* html_new_element(const char* tag, uint16_t tag_len){
    html_node_t* n = (html_node_t*)kmalloc(sizeof(html_node_t));
    if (!n) return 0;
    memset(n, 0, sizeof(*n));
    n->type = HNODE_ELEMENT;
    n->tag = tag;
    n->tag_len = tag_len;
    return n;
}

html_node_t* html_new_text(const char* text, uint32_t len){
    html_node_t* n = (html_node_t*)kmalloc(sizeof(html_node_t));
    if (!n) return 0;
    memset(n, 0, sizeof(*n));
    n->type = HNODE_TEXT;
    n->text = text;
    n->text_len = len;
    return n;
}

void html_append_child(html_node_t* parent, html_node_t* child){
    if (!parent || !child) return;
    child->parent = parent;

    if (!parent->first_child){
        parent->first_child = child;
        return;
    }
    html_node_t* it = parent->first_child;
    while (it->next_sibling) it = it->next_sibling;
    it->next_sibling = child;
}

static bool match_ci_slice(const char* a, uint16_t an, const char* b){
    uint16_t bi = 0;
    while (b[bi]){
        if (bi >= an) return false;
        if (to_lower(a[bi]) != b[bi]) return false;
        bi++;
    }
    return (bi == an);
}

bool html_tag_eq(const html_node_t* n, const char* s){
    if (!n || n->type != HNODE_ELEMENT || !n->tag) return false;
    return match_ci_slice(n->tag, n->tag_len, s);
}