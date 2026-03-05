#include <ui/html/css.h>
#include <lib/string.h>
#include <kernel/printk.h>

typedef enum {
    T_EOF=0,
    T_LBRACE, T_RBRACE, T_COLON, T_SEMI, T_COMMA,
    T_DOT, T_HASH,
    T_IDENT,
    T_HASHCOLOR
} tok_type_t;

typedef struct {
    tok_type_t t;
    const char* s;
    int len;
} tok_t;

typedef struct {
    const char* p;
    tok_t cur;
} lex_t;

static bool is_ws(char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'; }
static bool is_ident_start(char c){
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='-';
}
static bool is_ident_char(char c){
    return is_ident_start(c)||(c>='0'&&c<='9');
}
static int hexv(char c){
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+(c-'a');
    if(c>='A'&&c<='F') return 10+(c-'A');
    return -1;
}

static void skip_ws_comments(lex_t* L){
    for(;;){
        while(is_ws(*L->p)) L->p++;
        if(L->p[0]=='/' && L->p[1]=='*'){
            L->p += 2;
            while(L->p[0] && !(L->p[0]=='*' && L->p[1]=='/')) L->p++;
            if(L->p[0]) L->p += 2;
            continue;
        }
        break;
    }
}

static tok_t next_tok(lex_t* L){
    skip_ws_comments(L);
    tok_t tk = {T_EOF, L->p, 0};
    char c = *L->p;
    if(!c){ tk.t=T_EOF; return tk; }

    // single char tokens
    if(c=='{'){ L->p++; tk.t=T_LBRACE; tk.len=1; return tk; }
    if(c=='}'){ L->p++; tk.t=T_RBRACE; tk.len=1; return tk; }
    if(c==':'){ L->p++; tk.t=T_COLON; tk.len=1; return tk; }
    if(c==';'){ L->p++; tk.t=T_SEMI; tk.len=1; return tk; }
    if(c==','){ L->p++; tk.t=T_COMMA; tk.len=1; return tk; }
    if(c=='.'){ L->p++; tk.t=T_DOT; tk.len=1; return tk; }
    if(c=='#'){
        // could be #id or #rrggbb
        const char* start = L->p;
        L->p++; // eat '#'
        int h0 = hexv(*L->p);
        if(h0>=0){
            // read hex sequence
            const char* q = L->p;
            int n=0;
            while(hexv(*q)>=0){ q++; n++; }
            // accept 3 or 6 only as color
            if(n==3 || n==6){
                tk.t = T_HASHCOLOR;
                tk.s = start;
                tk.len = 1+n;
                L->p = q;
                return tk;
            }
        }
        // else treat as '#'
        tk.t=T_HASH; tk.s=start; tk.len=1;
        return tk;
    }

    // ident
    if(is_ident_start(c)){
        const char* start=L->p;
        L->p++;
        while(is_ident_char(*L->p)) L->p++;
        tk.t=T_IDENT; tk.s=start; tk.len=(int)(L->p-start);
        return tk;
    }

    // unknown char: skip it (robust)
    L->p++;
    tk.t = T_EOF;
    return next_tok(L);
}

static void lex_init(lex_t* L, const char* text){
    L->p = text ? text : "";
    L->cur.t = T_EOF;
    L->cur.s = L->p;
    L->cur.len = 0;
    L->cur = next_tok(L);
}
static void eat(lex_t* L){ L->cur = next_tok(L); }
static bool accept(lex_t* L, tok_type_t t){
    if(L->cur.t==t){ eat(L); return true; }
    return false;
}

static void copy_ident(char out[CSS_IDENT_MAX], const char* s, int len){
    int n = len;
    if(n >= CSS_IDENT_MAX) n = CSS_IDENT_MAX-1;
    for(int i=0;i<n;i++) out[i]=s[i];
    out[n]=0;
}

static css_property_t prop_from_ident(const char* s, int len){
    // background-color
    if(len==16 && !memcmp(s, "background-color", 16)) return CSS_PROP_BACKGROUND_COLOR;
    if(len==5  && !memcmp(s, "color", 5)) return CSS_PROP_COLOR;
    return CSS_PROP_UNKNOWN;
}

static uint32_t named_color(const char* s, int len, bool* ok){
    *ok = true;
    if(len==3 && !memcmp(s,"red",3))   return 0x00FF0000;
    if(len==5 && !memcmp(s,"white",5)) return 0x00FFFFFF;
    if(len==5 && !memcmp(s,"black",5)) return 0x00000000;
    if(len==4 && !memcmp(s,"blue",4))  return 0x000000FF;
    if(len==5 && !memcmp(s,"green",5)) return 0x0000FF00;
    *ok = false;
    return 0;
}

static bool parse_color_token(tok_t tk, uint32_t* out){
    if(tk.t==T_HASHCOLOR){
        // tk.s like "#abc" or "#aabbcc"
        const char* p = tk.s+1;
        int n = tk.len-1;
        if(n==3){
            int r=hexv(p[0]), g=hexv(p[1]), b=hexv(p[2]);
            if(r<0||g<0||b<0) return false;
            r = r*17; g=g*17; b=b*17;
            *out = ((uint32_t)r<<16)|((uint32_t)g<<8)|((uint32_t)b);
            return true;
        }else if(n==6){
            int r1=hexv(p[0]), r2=hexv(p[1]);
            int g1=hexv(p[2]), g2=hexv(p[3]);
            int b1=hexv(p[4]), b2=hexv(p[5]);
            if(r1<0||r2<0||g1<0||g2<0||b1<0||b2<0) return false;
            int r=(r1<<4)|r2, g=(g1<<4)|g2, b=(b1<<4)|b2;
            *out = ((uint32_t)r<<16)|((uint32_t)g<<8)|((uint32_t)b);
            return true;
        }
    }
    return false;
}

static void skip_value_until(lex_t* L){
    // skip tokens until ';' or '}' or EOF
    while(L->cur.t!=T_EOF && L->cur.t!=T_SEMI && L->cur.t!=T_RBRACE){
        eat(L);
    }
}

static bool node_has_class(const html_node_t* n, const char* cls){
    if(!n || !n->class_name || !n->class_len) return false;
    // simple space-separated match
    const char* s = n->class_name;
    int len = (int)n->class_len;
    int i=0;
    int cls_len = (int)strlen(cls);
    while(i < len){
        while(i<len && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
        int start=i;
        while(i<len && s[i]!=' '&&s[i]!='\t'&&s[i]!='\n'&&s[i]!='\r') i++;
        int wlen=i-start;
        if(wlen==cls_len && !memcmp(s+start, cls, cls_len)) return true;
    }
    return false;
}

static bool node_id_eq(const html_node_t* n, const char* id){
    if(!n || !n->id || !n->id_len) return false;
    int ilen = (int)n->id_len;
    int tlen = (int)strlen(id);
    return ilen==tlen && !memcmp(n->id, id, tlen);
}

static bool node_tag_eq2(const html_node_t* n, const char* tag){
    if(!n || !n->tag || !n->tag_len) return false;
    int nlen=(int)n->tag_len;
    int tlen=(int)strlen(tag);
    return nlen==tlen && !memcmp(n->tag, tag, tlen);
}

static bool selector_match(const html_node_t* n, const css_selector_t* sel){
    if(!n || n->type!=HNODE_ELEMENT) return false;
    switch(sel->type){
        case CSS_SEL_TAG: return node_tag_eq2(n, sel->name);
        case CSS_SEL_CLASS: return node_has_class(n, sel->name);
        case CSS_SEL_ID: return node_id_eq(n, sel->name);
        case CSS_SEL_TAG_CLASS:
            return node_tag_eq2(n, sel->tag) && node_has_class(n, sel->name);
        case CSS_SEL_TAG_ID:
            return node_tag_eq2(n, sel->tag) && node_id_eq(n, sel->name);
        default: return false;
    }
}

static int selector_specificity(const css_selector_t* sel){
    // minimal specificity scoring
    switch(sel->type){
        case CSS_SEL_ID: return 100;
        case CSS_SEL_TAG_ID: return 110;
        case CSS_SEL_CLASS: return 10;
        case CSS_SEL_TAG_CLASS: return 11;
        case CSS_SEL_TAG: return 1;
        default: return 0;
    }
}

static bool parse_one_selector(lex_t* L, css_selector_t* out){
    // supports: div, .box, #main, div.box, div#main
    css_selector_t s; memset(&s,0,sizeof(s));
    s.type = CSS_SEL_ANY;

    // optional tag ident
    if(L->cur.t==T_IDENT){
        s.type = CSS_SEL_TAG;
        copy_ident(s.name, L->cur.s, L->cur.len); // name used for tag-only
        eat(L);
    }

    // optional .class or #id (single, first version)
    if(accept(L, T_DOT)){
        if(L->cur.t!=T_IDENT) return false;
        if(s.type==CSS_SEL_TAG){
            s.type = CSS_SEL_TAG_CLASS;
            copy_ident(s.tag, s.name, (int)strlen(s.name));
            copy_ident(s.name, L->cur.s, L->cur.len); // class name
        }else{
            s.type = CSS_SEL_CLASS;
            copy_ident(s.name, L->cur.s, L->cur.len);
        }
        eat(L);
        *out = s;
        return true;
    }

    if(accept(L, T_HASH)){
        if(L->cur.t!=T_IDENT) return false;
        if(s.type==CSS_SEL_TAG){
            s.type = CSS_SEL_TAG_ID;
            copy_ident(s.tag, s.name, (int)strlen(s.name));
            copy_ident(s.name, L->cur.s, L->cur.len); // id
        }else{
            s.type = CSS_SEL_ID;
            copy_ident(s.name, L->cur.s, L->cur.len);
        }
        eat(L);
        *out = s;
        return true;
    }

    if(s.type==CSS_SEL_TAG){
        *out = s;
        return true;
    }

    // no usable selector
    return false;
}

void css_parse(const char* css_text, css_stylesheet_t* out){
    if(!out) return;
    out->rule_count = 0;

    lex_t L; lex_init(&L, css_text);

    while(L.cur.t != T_EOF){
        // Try parse selector
        css_selector_t sel;
        if(!parse_one_selector(&L, &sel)){
            // skip garbage until '{' or ';'
            while(L.cur.t!=T_EOF && L.cur.t!=T_LBRACE && L.cur.t!=T_SEMI) eat(&L);
            accept(&L, T_SEMI);
            if(accept(&L, T_LBRACE)){
                // skip block
                int depth=1;
                while(L.cur.t!=T_EOF && depth>0){
                    if(accept(&L, T_LBRACE)) depth++;
                    else if(accept(&L, T_RBRACE)) depth--;
                    else eat(&L);
                }
            }
            continue;
        }

        // skip selector list commas (we ignore extra selectors for now)
        while(accept(&L, T_COMMA)){
            // skip next selector quickly
            css_selector_t dummy;
            if(!parse_one_selector(&L, &dummy)) break;
        }

        if(!accept(&L, T_LBRACE)){
            // malformed, skip
            continue;
        }

        if(out->rule_count >= CSS_MAX_RULES){
            // skip block if full
            int depth=1;
            while(L.cur.t!=T_EOF && depth>0){
                if(accept(&L, T_LBRACE)) depth++;
                else if(accept(&L, T_RBRACE)) depth--;
                else eat(&L);
            }
            continue;
        }

        css_rule_t* r = &out->rules[out->rule_count++];
        memset(r,0,sizeof(*r));
        r->sel = sel;

        // declarations
        while(L.cur.t!=T_EOF && L.cur.t!=T_RBRACE){
            if(L.cur.t!=T_IDENT){
                // stray token
                eat(&L);
                continue;
            }
            css_property_t prop = prop_from_ident(L.cur.s, L.cur.len);
            eat(&L);

            if(!accept(&L, T_COLON)){
                // malformed, skip to ';' or '}'
                skip_value_until(&L);
                accept(&L, T_SEMI);
                continue;
            }

            // value token (very minimal)
            uint32_t col=0;
            bool ok=false;

            if(parse_color_token(L.cur, &col)){
                ok = true;
                eat(&L);
            }else if(L.cur.t==T_IDENT){
                col = named_color(L.cur.s, L.cur.len, &ok);
                eat(&L);
            }else{
                // unknown value start
                ok = false;
            }

            // skip rest of value (e.g., !important etc.)
            skip_value_until(&L);

            if(prop!=CSS_PROP_UNKNOWN && ok && r->decl_count < CSS_MAX_DECLS){
                css_decl_t* d = &r->decls[r->decl_count++];
                d->prop = prop;
                d->value = col;
                d->has_value = true;
            }

            accept(&L, T_SEMI);
        }
        accept(&L, T_RBRACE);
    }
}

static void apply_rule_to_node(html_node_t* n, const css_rule_t* r){
    for(int i=0;i<r->decl_count;i++){
        const css_decl_t* d = &r->decls[i];
        if(!d->has_value) continue;
        if(d->prop==CSS_PROP_BACKGROUND_COLOR){
            n->css_bg = d->value;
            n->css_has_bg = true;
        }else if(d->prop==CSS_PROP_COLOR){
            n->css_fg = d->value;
            n->css_has_fg = true;
        }
    }
}

static void apply_node(html_node_t* n, const css_stylesheet_t* sheet){
    if(!n || n->type!=HNODE_ELEMENT) return;

    // basic cascade: higher specificity wins; if equal, later rule wins
    int best_bg_spec = -1;
    int best_fg_spec = -1;
    uint32_t best_bg = 0, best_fg = 0;
    bool has_bg=false, has_fg=false;

    for(int i=0;i<sheet->rule_count;i++){
        const css_rule_t* r = &sheet->rules[i];
        if(!selector_match(n, &r->sel)) continue;
        int spec = selector_specificity(&r->sel);

        for(int k=0;k<r->decl_count;k++){
            const css_decl_t* d = &r->decls[k];
            if(!d->has_value) continue;

            if(d->prop==CSS_PROP_BACKGROUND_COLOR){
                if(spec > best_bg_spec || spec==best_bg_spec){
                    best_bg_spec = spec;
                    best_bg = d->value;
                    has_bg = true;
                }
            }else if(d->prop==CSS_PROP_COLOR){
                if(spec > best_fg_spec || spec==best_fg_spec){
                    best_fg_spec = spec;
                    best_fg = d->value;
                    has_fg = true;
                }
            }
        }
    }

    if(has_bg){ n->css_bg = best_bg; n->css_has_bg = true; }
    if(has_fg){ n->css_fg = best_fg; n->css_has_fg = true; }
}

static void dfs_apply(html_node_t* n, const css_stylesheet_t* sheet){
    if(!n) return;
    if(n->type==HNODE_ELEMENT) apply_node(n, sheet);
    for(html_node_t* c=n->first_child; c; c=c->next_sibling){
        dfs_apply(c, sheet);
    }
}

void css_apply_styles(html_node_t* root, const css_stylesheet_t* sheet){
    if(!root || !sheet) return;
    dfs_apply(root, sheet);
}