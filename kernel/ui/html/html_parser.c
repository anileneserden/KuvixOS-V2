#include <stddef.h>
#include <ui/html/html_parser.h>
#include <ui/html/html_tokenizer.h>
#include <kernel/memory/kmalloc.h>

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

bool html_parse(html_doc_t* doc, const char* data, uint32_t size){
    if (!doc || !data) return false;

    doc->root = html_new_element("document", 8);
    if (!doc->root) return false;

    html_node_t* stack[STACK_MAX];
    int sp = 0;
    stack[sp++] = doc->root;

    html_lex_t lx;
    html_lex_init(&lx, data, size);

    html_tok_t t;
    while (html_lex_next(&lx, &t)){
        if (t.type == HTOK_EOF) break;

        if (t.type == HTOK_TEXT){
            html_node_t* tn = html_new_text(t.start, t.len);
            if (tn) html_append_child(stack[sp-1], tn);
            continue;
        }

        if (t.type == HTOK_TAG_OPEN){
            if (!t.tag || t.tag_len==0) continue;

            html_node_t* el = html_new_element(t.tag, (uint16_t)t.tag_len);
            if (!el) continue;

            if (t.href && t.href_len){
                el->href = t.href;
                el->href_len = (uint16_t)t.href_len;
            }

            el->self_closing = t.self_closing || is_void_tag(t.tag, (uint16_t)t.tag_len);

            html_append_child(stack[sp-1], el);

            if (!el->self_closing && sp < STACK_MAX){
                stack[sp++] = el;
            }
            continue;
        }

        if (t.type == HTOK_TAG_CLOSE){
            if (!t.tag || t.tag_len==0) continue;

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

// Düğümleri recursive olarak temizleyen yardımcı fonksiyon
static void html_free_node(html_node_t* node) {
    if (!node) return;

    // 1. Önce varsa çocukları (ilk çocuğu) temizle
    // Senin yapında 'first_child' kullanılmış
    if (node->first_child) {
        html_free_node(node->first_child);
    }

    // 2. Varsa kardeşleri (next sibling) temizle
    // HATA: Senin struct'ında bu alanın adı 'next_sibling' veya benzeri olabilir.
    // html_append_child fonksiyonun nasıl bağladığına bakarak burayı 'sibling' olarak güncelledim.
    if (node->next_sibling) { 
        html_free_node(node->next_sibling);
    }

    // 3. Düğümün içindeki dinamik alanları temizle
    // html_parse içinde 'cand->tag' kullandığın için 'tag_name' yerine 'tag' yazdım.
    // Eğer tag ve text alanlarını kmalloc ile ayırmıyorsan (sadece pointer ise) buraları silebiliriz.
    if (node->type == HNODE_ELEMENT && node->tag) {
        // kfree(node->tag); // Eğer tag ismi kmalloc ile kopyalanmışsa aktif et
    }
    
    // 4. En son düğümün kendisini temizle
    kfree(node);
}

void html_free(html_doc_t* doc) {
    if (!doc || !doc->root) return;

    html_free_node(doc->root);
    doc->root = NULL; 
}