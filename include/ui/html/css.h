#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <ui/html/html_dom.h>

#define CSS_MAX_RULES 256
#define CSS_MAX_DECLS 16
#define CSS_IDENT_MAX 64

typedef enum {
    CSS_SEL_ANY = 0,
    CSS_SEL_TAG,
    CSS_SEL_CLASS,
    CSS_SEL_ID,
    CSS_SEL_TAG_CLASS, // div.box
    CSS_SEL_TAG_ID     // div#id
} css_selector_type_t;

typedef struct {
    css_selector_type_t type;
    char tag[CSS_IDENT_MAX];    // optional
    char name[CSS_IDENT_MAX];   // class or id or tag-only
} css_selector_t;

typedef enum {
    CSS_PROP_UNKNOWN = 0,
    CSS_PROP_BACKGROUND_COLOR,
    CSS_PROP_COLOR
} css_property_t;

typedef struct {
    css_property_t prop;
    uint32_t value;     // packed color
    bool has_value;
} css_decl_t;

typedef struct {
    css_selector_t sel;
    css_decl_t decls[CSS_MAX_DECLS];
    int decl_count;
} css_rule_t;

typedef struct {
    css_rule_t rules[CSS_MAX_RULES];
    int rule_count;
} css_stylesheet_t;

// Parse CSS text into stylesheet (unsupported things are ignored)
void css_parse(const char* css_text, css_stylesheet_t* out);

// Apply stylesheet to DOM (writes node->css_bg/css_fg flags)
void css_apply_styles(html_node_t* root, const css_stylesheet_t* sheet);