#include <ui/html/html_render.h>
#include <kernel/drivers/video/gfx.h>

#define HTML_CHAR_W 8
#define HTML_LINE_H 16

typedef struct {
    int x, y;
    int max_w;
    int start_x;

    uint32_t color_text;   // default text
    uint32_t color_link;   // default link

    uint32_t cur_text;     // current/inherited text color
} html_render_ctx_t;

static bool is_space(char c){
    return (c==' '||c=='\t'||c=='\r'||c=='\n');
}

static void newline(html_render_ctx_t* ctx){
    ctx->x = ctx->start_x;
    ctx->y += HTML_LINE_H;
}

static void draw_underline(int x, int y, int w, uint32_t color){
    int uy = y + HTML_LINE_H - 2;
    gfx_draw_line(x, uy, x + w - 1, uy, color);
}

/*
 * Local fill helper (in case you don't have gfx_fill_rect).
 * If you already have gfx_fill_rect(), you can replace calls to fill_rect()
 * with gfx_fill_rect() directly.
 */
static void fill_rect(int x, int y, int w, int h, uint32_t color){
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++){
        gfx_draw_line(x, y + yy, x + w - 1, y + yy, color);
    }
}

static void render_text_slice(html_render_ctx_t* ctx, const char* s, uint32_t len, uint32_t color, bool underline){
    if (!s || !len) return;

    uint32_t i = 0;
    while (i < len){
        while (i < len && is_space(s[i])) i++;
        if (i >= len) break;

        uint32_t word_start = i;
        while (i < len && !is_space(s[i])) i++;
        uint32_t word_len = i - word_start;

        int word_px = (int)word_len * HTML_CHAR_W;

        if (ctx->x + word_px > ctx->start_x + ctx->max_w){
            newline(ctx);
        }

        char tmp[256];
        uint32_t cp = word_len;
        if (cp >= sizeof(tmp)) cp = sizeof(tmp)-1;

        for (uint32_t k=0;k<cp;k++) tmp[k]=s[word_start+k];
        tmp[cp]=0;

        int draw_x = ctx->x;
        gfx_draw_text_utf8(draw_x, ctx->y, color, tmp);

        if (underline){
            draw_underline(draw_x, ctx->y, (int)cp * HTML_CHAR_W, color);
        }

        ctx->x += (int)cp * HTML_CHAR_W;

        // space
        if (ctx->x + HTML_CHAR_W > ctx->start_x + ctx->max_w){
            newline(ctx);
        } else {
            ctx->x += HTML_CHAR_W;
        }
    }
}

static void render_node(html_render_ctx_t* ctx, const html_node_t* n, bool in_link){
    if (!n) return;

    if (n->type == HNODE_TEXT){
        // Text color is inherited via ctx->cur_text
        uint32_t color = ctx->cur_text;
        render_text_slice(ctx, n->text, n->text_len, color, in_link);
        return;
    }

    // --- ELEMENT NODE ---
    // Push inherited text color
    uint32_t saved_text = ctx->cur_text;

    // Apply CSS 'color' if present (inheritable)
    // NOTE: requires adding these fields to html_node_t:
    //   uint32_t css_fg; bool css_has_fg;
    if (n->css_has_fg){
        ctx->cur_text = n->css_fg;
    } else {
        // Default link color if we're entering an <a> and no explicit color set
        if (html_tag_eq(n, "a")){
            ctx->cur_text = ctx->color_link;
        }
    }

    bool block_start = false;
    bool block_end   = false;

    if (html_tag_eq(n, "p") || html_tag_eq(n, "div")){
        block_start = true; block_end = true;
    }
    if (html_tag_eq(n, "h1")){
        block_start = true; block_end = true;
    }
    if (html_tag_eq(n, "br")){
        newline(ctx);
        // Pop color
        ctx->cur_text = saved_text;
        return;
    }

    if (block_start){
        if (ctx->x != ctx->start_x) newline(ctx);

        // Small extra spacing before blocks (your original behavior)
        if (html_tag_eq(n, "p") || html_tag_eq(n, "div")) ctx->y += 4;
        if (html_tag_eq(n, "h1")) ctx->y += 8;

        // Very simple background-color proof:
        // draw a 1-line stripe behind the start line of the block.
        // NOTE: requires adding these fields to html_node_t:
        //   uint32_t css_bg; bool css_has_bg;
        if (n->css_has_bg){
            int bx = ctx->start_x;
            int by = ctx->y;
            int bw = ctx->max_w;
            int bh = HTML_LINE_H;
            fill_rect(bx, by, bw, bh, n->css_bg);
        }
    }

    bool now_link = in_link || html_tag_eq(n, "a");

    for (const html_node_t* ch = n->first_child; ch; ch = ch->next_sibling){
        render_node(ctx, ch, now_link);
    }

    if (block_end){
        if (ctx->x != ctx->start_x) newline(ctx);
        ctx->y += 6;
    }

    // Pop inherited text color
    ctx->cur_text = saved_text;
}

void html_render_doc(const html_doc_t* doc, int x, int y, int w){
    if (!doc || !doc->root) return;

    const html_node_t* start = doc->root;

    if (doc->has_body && doc->body) {
        start = doc->body;
    }

    html_render_node(start, x, y, w);
}

void html_render_node(const html_node_t* node, int x, int y, int w){
    if (!node) return;

    html_render_ctx_t ctx;
    ctx.start_x = x;
    ctx.x = x;
    ctx.y = y;
    ctx.max_w = w;

    ctx.color_text = 0xFFFFFF;
    ctx.color_link = 0x33A0FF;

    ctx.cur_text = ctx.color_text;

    render_node(&ctx, node, false);
}