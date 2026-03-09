#include <ui/html/layout.h>
#include <ui/html/html_dom.h>
#include <lib/string.h>

#define HTML_CHAR_W 8
#define HTML_LINE_H 16

static bool is_space(char c){
    return (c==' '||c=='\t'||c=='\r'||c=='\n');
}

static int clampi(int v, int a, int b){
    if(v<a) return a;
    if(v>b) return b;
    return v;
}

static int css_width_px(const html_node_t* n, int parent_w){
    if(!n || !n->css_has_w) return -1;
    if(n->css_w_unit==CSS_UNIT_PX) return n->css_w;
    if(n->css_w_unit==CSS_UNIT_PCT) return (parent_w * n->css_w) / 100;
    return -1;
}

static int css_height_px(const html_node_t* n, int viewport_h){
    if(!n || !n->css_has_h) return -1;
    if(n->css_h_unit==CSS_UNIT_PX) return n->css_h;
    if(n->css_h_unit==CSS_UNIT_VH) return (viewport_h * n->css_h) / 100;
    return -1;
}

static void get_padding(const html_node_t* n, int* pl, int* pr, int* pt, int* pb){
    *pl = *pr = *pt = *pb = 0;
    if(!n || !n->css_has_pad) return;
    *pl = n->css_pad_l;
    *pr = n->css_pad_r;
    *pt = n->css_pad_t;
    *pb = n->css_pad_b;
}

static void get_margin(const html_node_t* n, int* ml, int* mr, int* mt, int* mb){
    *ml = *mr = *mt = *mb = 0;
    if(!n || !n->css_has_mar) return;
    *ml = n->css_mar_l;
    *mr = n->css_mar_r;
    *mt = n->css_mar_t;
    *mb = n->css_mar_b;
}

static int measure_text_lines(const char* s, uint32_t len, int avail_w){
    if(!s || !len) return 0;
    int max_chars = (avail_w <= 0) ? 0 : (avail_w / HTML_CHAR_W);
    if(max_chars <= 0) max_chars = 1;

    int lines = 0;
    uint32_t i = 0;
    int cur = 0;

    while(i < len){
        while(i < len && is_space(s[i])) i++;
        if(i >= len) break;

        uint32_t word_start = i;
        while(i < len && !is_space(s[i])) i++;
        int wlen = (int)(i - word_start);
        if(wlen <= 0) continue;

        if(lines == 0) lines = 1;

        if(cur == 0){
            // place word
            if(wlen >= max_chars){
                // very long word: occupies at least one line
                cur = clampi(wlen, 1, max_chars);
            }else{
                cur = wlen;
            }
        }else{
            // need one space
            if(cur + 1 + wlen > max_chars){
                lines++;
                cur = (wlen >= max_chars) ? max_chars : wlen;
            }else{
                cur += 1 + wlen;
            }
        }
    }
    return lines;
}

static int measure_subtree_height(const html_node_t* n, int content_w, int viewport_h);

// measure element height (auto) by children text + block children stacking
static int measure_element_auto_height(const html_node_t* n, int content_w, int viewport_h){
    if(!n) return 0;

    int pl, pr, pt, pb;
    get_padding(n, &pl, &pr, &pt, &pb);

    int inner_w = content_w - pl - pr;
    if(inner_w < HTML_CHAR_W) inner_w = HTML_CHAR_W;

    int y = 0;

    for(const html_node_t* ch = n->first_child; ch; ch = ch->next_sibling){
        if(ch->type == HNODE_TEXT){
            int lines = measure_text_lines(ch->text, ch->text_len, inner_w);
            y += lines * HTML_LINE_H;
        }else if(ch->type == HNODE_ELEMENT){
            // treat as block for now
            int h = measure_subtree_height(ch, inner_w, viewport_h);
            y += h;
        }
    }

    if(y == 0) y = HTML_LINE_H; // minimum line for empty blocks
    return y + pt + pb;
}

static int measure_subtree_height(const html_node_t* n, int content_w, int viewport_h){
    if(!n) return 0;
    if(n->type == HNODE_TEXT){
        int lines = measure_text_lines(n->text, n->text_len, content_w);
        return lines * HTML_LINE_H;
    }

    int fixed_h = css_height_px(n, viewport_h);
    if(fixed_h >= 0) return fixed_h;

    // auto by children
    return measure_element_auto_height(n, content_w, viewport_h);
}

static bool is_block_like(const html_node_t* n){
    if(!n || n->type != HNODE_ELEMENT) return false;
    // minimal: div, p, h1, body
    if(html_tag_eq(n, "div")) return true;
    if(html_tag_eq(n, "p"))   return true;
    if(html_tag_eq(n, "h1"))  return true;
    if(html_tag_eq(n, "body"))return true;
    return true; // default all elements treated as block in this MVP
}

static html_node_t* first_element_child(html_node_t* n){
    for(html_node_t* ch = n ? n->first_child : 0; ch; ch = ch->next_sibling){
        if(ch->type == HNODE_ELEMENT) return ch;
    }
    return 0;
}

static void layout_block_children(html_node_t* parent, int viewport_x, int viewport_y, int viewport_w, int viewport_h);

static void layout_one_block(html_node_t* n, int x, int y, int avail_w, int viewport_h){
    if(!n) return;

    int ml,mr,mt,mb;
    get_margin(n, &ml,&mr,&mt,&mb);

    int pl,pr,pt,pb;
    get_padding(n, &pl,&pr,&pt,&pb);

    int w = css_width_px(n, avail_w);
    if(w < 0) w = avail_w;

    // handle auto margins horizontally (center)
    int left = x + ml;
    int right_space = avail_w - w - ml - mr;
    if(right_space < 0) right_space = 0;

    if(n->css_mar_l_auto && n->css_mar_r_auto){
        left = x + (avail_w - w) / 2;
    }else if(n->css_mar_l_auto){
        left = x + right_space; // push to right
    }else if(n->css_mar_r_auto){
        left = x + ml; // default left
    }

    n->box.x = left;
    n->box.y = y + mt;
    n->box.w = w;
    // height later
    int h = measure_subtree_height(n, w - pl - pr, viewport_h);
    n->box.h = h;

    // layout its children inside content box
    layout_block_children(n, n->box.x + pl, n->box.y + pt, n->box.w - pl - pr, viewport_h);
}

static void layout_block_children(html_node_t* parent, int viewport_x, int viewport_y, int viewport_w, int viewport_h){
    if(!parent) return;

    int cur_y = viewport_y;

    for(html_node_t* ch = parent->first_child; ch; ch = ch->next_sibling){
        if(ch->type != HNODE_ELEMENT) continue;
        if(!is_block_like(ch)) continue;

        int ml,mr,mt,mb;
        get_margin(ch, &ml,&mr,&mt,&mb);

        layout_one_block(ch, viewport_x, cur_y, viewport_w, viewport_h);

        cur_y = ch->box.y + ch->box.h + mb;
    }
}

void html_layout_build(html_node_t* root, int viewport_x, int viewport_y, int viewport_w, int viewport_h){
    if(!root) return;

    // root box
    root->box.x = viewport_x;
    root->box.y = viewport_y;
    root->box.w = viewport_w;
    root->box.h = viewport_h;

    // Special: body flex center (only if root==body or if body called)
    if(root->type==HNODE_ELEMENT && html_tag_eq(root, "body") && root->css_has_display && root->css_display==CSS_DISPLAY_FLEX){
        html_node_t* child = first_element_child(root);
        if(child){
            // compute child size
            int pl,pr,pt,pb;
            get_padding(root, &pl,&pr,&pt,&pb);

            int inner_x = viewport_x + pl;
            int inner_y = viewport_y + pt;
            int inner_w = viewport_w - pl - pr;
            int inner_h = viewport_h - pt - pb;

            if(inner_w < 1) inner_w = 1;
            if(inner_h < 1) inner_h = 1;

            int cw = css_width_px(child, inner_w);
            if(cw < 0) cw = inner_w;

            int chh = css_height_px(child, viewport_h);
            if(chh < 0){
                // auto measure
                int cpl,cpr,cpt,cpb;
                get_padding(child,&cpl,&cpr,&cpt,&cpb);
                chh = measure_subtree_height(child, cw - cpl - cpr, viewport_h);
            }

            int cx = inner_x;
            int cy = inner_y;

            if(root->css_justify == CSS_JUSTIFY_CENTER){
                cx = inner_x + (inner_w - cw)/2;
            }
            if(root->css_align == CSS_ALIGN_CENTER){
                cy = inner_y + (inner_h - chh)/2;
            }

            child->box.x = cx;
            child->box.y = cy;
            child->box.w = cw;
            child->box.h = chh;

            // layout children inside child
            int cpl,cpr,cpt,cpb;
            get_padding(child,&cpl,&cpr,&cpt,&cpb);
            layout_block_children(child, child->box.x + cpl, child->box.y + cpt, child->box.w - cpl - cpr, viewport_h);
        }
        return;
    }

    // Normal block flow
    layout_block_children(root, viewport_x, viewport_y, viewport_w, viewport_h);
}