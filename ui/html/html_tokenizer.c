#include <ui/html/html_tokenizer.h>
#include <lib/string.h>
#include <stdint.h>

static bool is_space(char c){
    return (c==' '||c=='\t'||c=='\r'||c=='\n');
}
static char to_lower(char c){
    if (c>='A' && c<='Z') return (char)(c - 'A' + 'a');
    return c;
}

void html_lex_init(html_lex_t* lx, const char* data, uint32_t size){
    lx->p = data;
    lx->end = data + size;
}

static bool match_kw_ci(const char* s, uint32_t n, const char* kw){
    uint32_t i=0;
    for (; kw[i]; i++){
        if (i>=n) return false;
        if (to_lower(s[i]) != kw[i]) return false;
    }
    return (i==n);
}

static void skip_spaces(html_lex_t* lx){
    while (lx->p < lx->end && is_space(*lx->p)) lx->p++;
}

static void try_parse_attrs(const char* p, const char* end, html_tok_t* out){
    out->href = 0; out->href_len = 0;
    out->id = 0; out->id_len = 0;
    out->class_name = 0; out->class_len = 0;
    out->style = 0; out->style_len = 0;

    while (p < end){
        while (p<end && is_space(*p)) p++;
        if (p>=end) break;
        if (*p=='>' || (*p=='/' && (p+1)<end && p[1]=='>')) break;

        const char* name = p;
        while (p<end && !is_space(*p) && *p!='=' && *p!='>') p++;
        uint32_t name_len = (uint32_t)(p - name);

        while (p<end && is_space(*p)) p++;
        if (p<end && *p=='=') p++;
        while (p<end && is_space(*p)) p++;

        const char* val = p;
        uint32_t val_len = 0;

        if (p<end && (*p=='"' || *p=='\'')){
            char q = *p++;
            val = p;
            while (p<end && *p!=q) p++;
            val_len = (uint32_t)(p - val);
            if (p<end && *p==q) p++;
        } else {
            val = p;
            while (p<end && !is_space(*p) && *p!='>') p++;
            val_len = (uint32_t)(p - val);
        }

        if (match_kw_ci(name, name_len, "href")){
            out->href = val;
            out->href_len = (uint16_t)((val_len > 0xFFFF) ? 0xFFFF : val_len);
        } else if (match_kw_ci(name, name_len, "id")){
            out->id = val;
            out->id_len = (uint16_t)((val_len > 0xFFFF) ? 0xFFFF : val_len);
        } else if (match_kw_ci(name, name_len, "class")){
            out->class_name = val;
            out->class_len = (uint16_t)((val_len > 0xFFFF) ? 0xFFFF : val_len);
        } else if (match_kw_ci(name, name_len, "style")){
            out->style = val;
            out->style_len = (uint16_t)((val_len > 0xFFFF) ? 0xFFFF : val_len);
        }
    }
}

bool html_lex_next(html_lex_t* lx, html_tok_t* out){
    out->type = HTOK_EOF;
    out->start = 0; out->len = 0;
    out->tag = 0; out->tag_len = 0;
    out->href = 0; out->href_len = 0;
    out->self_closing = false;
    out->id = 0; out->id_len = 0;
    out->class_name = 0; out->class_len = 0;
    out->style = 0; out->style_len = 0;

    if (lx->p >= lx->end){
        out->type = HTOK_EOF;
        return true;
    }

    // TEXT
    if (*lx->p != '<'){
        const char* s = lx->p;
        while (lx->p < lx->end && *lx->p != '<') lx->p++;
        out->type = HTOK_TEXT;
        out->start = s;
        out->len = (uint32_t)(lx->p - s);
        return true;
    }

    // TAG
    const char* tag_start = lx->p;
    lx->p++; // '<'
    if (lx->p >= lx->end){
        out->type = HTOK_EOF;
        return true;
    }

    // comment ignore <!-- -->
    if ((lx->end - lx->p) >= 3 && lx->p[0]=='!' && lx->p[1]=='-' && lx->p[2]=='-'){
        lx->p += 3;
        while ((lx->end - lx->p) >= 3){
            if (lx->p[0]=='-' && lx->p[1]=='-' && lx->p[2]=='>'){
                lx->p += 3;
                return html_lex_next(lx, out);
            }
            lx->p++;
        }
        out->type = HTOK_EOF;
        return true;
    }

    bool closing = false;
    if (*lx->p == '/'){ closing = true; lx->p++; }

    skip_spaces(lx);

    const char* name = lx->p;
    while (lx->p < lx->end){
        char c = *lx->p;
        if (is_space(c) || c=='>' || c=='/') break;
        lx->p++;
    }
    uint32_t name_len = (uint32_t)(lx->p - name);

    const char* attr_begin = lx->p;

    // find '>'
    while (lx->p < lx->end && *lx->p != '>') lx->p++;
    if (lx->p >= lx->end){
        out->type = HTOK_EOF;
        return true;
    }

    // self close "/>"
    if ((lx->p > tag_start) && (lx->p[-1] == '/')) out->self_closing = true;

    // parse href within attr region
    try_parse_attrs(attr_begin, lx->p, out);

    lx->p++; // '>'

    out->start = tag_start;
    out->len = (uint32_t)(lx->p - tag_start);
    out->tag = name;
    out->tag_len = name_len;
    out->type = closing ? HTOK_TAG_CLOSE : HTOK_TAG_OPEN;
    return true;
}