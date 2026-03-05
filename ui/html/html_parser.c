#include <ui/html/html_parser.h>
#include <ui/html/html_dom.h>
#include <ui/html/html_tokenizer.h>

#define STACK_MAX 64

static char to_lower(char c){
    if (c>='A' && c<='Z') return (char)(c - 'A' + 'a');
    return c;
}

static bool slice_eq_ci(const char* a, uint16_t an, const char* b, uint16_t bn){
    if (an != bn) return false;
    for (uint16_t i=0;i<an;i++){
        if (to_lower(a[i]) != to_lower(b[i])) return false;
    }
    return true;
}

static bool is_void_tag(const char* tag, uint16_t n){
    // br, img, hr
    if (n==2 && (to_lower(tag[0])=='b') && (to_lower(tag[1])=='r')) return true;
    if (n==2 && (to_lower(tag[0])=='h') && (to_lower(tag[1])=='r')) return true;
    if (n==3 && (to_lower(tag[0])=='i') && (to_lower(tag[1])=='m') && (to_lower(tag[2])=='g')) return true;
    return false;
}

static bool tag_is(const html_tok_t* t, const char* lit){
    if (!t || !t->tag || !t->tag_len) return false;
    uint16_t ln = (uint16_t)0;
    while (lit[ln]) ln++;
    return slice_eq_ci(t->tag, (uint16_t)t->tag_len, lit, ln);
}

bool html_parse(html_doc_t* doc, const char* data, uint32_t size){
    if (!doc || !data) return false;

    // ✅ init
    doc->root = 0;
    doc->body = 0;
    doc->has_body = false;
    doc->title = 0;
    doc->title_len = 0;

    doc->root = html_new_element("document", 8);
    if (!doc->root) return false;

    html_node_t* stack[STACK_MAX];
    int sp = 0;
    stack[sp++] = doc->root;

    html_lex_t lx;
    html_lex_init(&lx, data, size);

    // ✅ parsing state
    bool in_head = false;
    bool in_body = false;
    bool in_title = false;

    html_tok_t t;
    while (html_lex_next(&lx, &t)){
        if (t.type == HTOK_EOF) break;

        if (t.type == HTOK_TEXT){
            // ✅ title capture (HEAD içinde <title>...</title>)
            if (in_title && !doc->title && t.start && t.len){
                doc->title = t.start;
                doc->title_len = (uint16_t)((t.len > 0xFFFF) ? 0xFFFF : t.len);
                continue; // title text node üretme (render istemiyoruz)
            }

            // ✅ head içindeki normal text'i render etme
            if (in_head && !in_body) {
                continue;
            }

            html_node_t* tn = html_new_text(t.start, t.len);
            if (tn) html_append_child(stack[sp-1], tn);
            continue;
        }

        if (t.type == HTOK_TAG_OPEN){
            if (!t.tag || t.tag_len==0) continue;

            // ✅ head/body/title flags
            if (tag_is(&t, "head"))  { in_head = true; }
            if (tag_is(&t, "body"))  { in_body = true; doc->has_body = true; }
            if (tag_is(&t, "title")) { if (in_head) in_title = true; }

            // ✅ head içindeki tag'leri render etmiyoruz, ama stack dengesi için element üretmek isteyebilirsin.
            // En minimal: head içindeyken (title hariç) element oluşturma.
            if (in_head && !in_body && !tag_is(&t, "title") && !tag_is(&t, "head") && !tag_is(&t, "html")) {
                // void/self-closing olsa da ignore
                continue;
            }

            html_node_t* el = html_new_element(t.tag, (uint16_t)t.tag_len);
            if (!el) continue;

            if (t.href && t.href_len){
                el->href = t.href;
                el->href_len = (uint16_t)t.href_len;
            }
            if (t.id && t.id_len){
                el->id = t.id;
                el->id_len = (uint16_t)t.id_len;
            }
            if (t.class_name && t.class_len){
                el->class_name = t.class_name;
                el->class_len = (uint16_t)t.class_len;
            }
            if (t.style && t.style_len){
                el->style = t.style;
                el->style_len = (uint16_t)t.style_len;
            }

            el->self_closing = t.self_closing || is_void_tag(t.tag, (uint16_t)t.tag_len);

            html_append_child(stack[sp-1], el);

            // ✅ body node pointer
            if (tag_is(&t, "body")) {
                doc->body = el;
            }

            if (!el->self_closing && sp < STACK_MAX){
                stack[sp++] = el;
            }
            continue;
        }

        if (t.type == HTOK_TAG_CLOSE){
            if (!t.tag || t.tag_len==0) continue;

            // ✅ close flags
            if (tag_is(&t, "head"))  { in_head = false; }
            if (tag_is(&t, "body"))  { in_body = false; }
            if (tag_is(&t, "title")) { in_title = false; }

            // en yakın eşleşeni bulup pop
            for (int i = sp-1; i >= 1; i--){
                html_node_t* cand = stack[i];
                if (cand->type == HNODE_ELEMENT &&
                    slice_eq_ci(cand->tag, cand->tag_len, t.tag, (uint16_t)t.tag_len)) {
                    sp = i; // cand üstünü at
                    break;
                }
            }
            continue;
        }
    }

    return true;
}