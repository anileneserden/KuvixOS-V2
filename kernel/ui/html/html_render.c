#include <ui/html/html_render.h>
#include <kernel/drivers/video/gfx.h>

#define HTML_CHAR_W 8
#define HTML_LINE_H 16

typedef struct {
    int x, y;
    int max_w;
    int start_x;
    uint32_t color_text;
    uint32_t color_link;
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
        uint32_t color = in_link ? ctx->color_link : ctx->color_text;
        render_text_slice(ctx, n->text, n->text_len, color, in_link);
        return;
    }

    // element
    bool block_start = false;
    bool block_end = false;

    if (html_tag_eq(n, "p") || html_tag_eq(n, "div")){
        block_start = true; block_end = true;
    }
    if (html_tag_eq(n, "h1")){
        block_start = true; block_end = true;
    }
    if (html_tag_eq(n, "br")){
        newline(ctx);
        return;
    }

    if (block_start){
        if (ctx->x != ctx->start_x) newline(ctx);
        if (html_tag_eq(n, "p") || html_tag_eq(n, "div")) ctx->y += 4;
        if (html_tag_eq(n, "h1")) ctx->y += 8;
    }

    bool now_link = in_link || html_tag_eq(n, "a");

    for (const html_node_t* ch = n->first_child; ch; ch = ch->next_sibling){
        render_node(ctx, ch, now_link);
    }

    if (block_end){
        if (ctx->x != ctx->start_x) newline(ctx);
        ctx->y += 6;
    }
}

void html_render_doc(const html_doc_t* doc, int x, int y, int w){
    if (!doc || !doc->root) return;

    html_render_ctx_t ctx;
    ctx.start_x = x;
    ctx.x = x;
    ctx.y = y;
    ctx.max_w = w;
    ctx.color_text = 0xFFFFFF;
    ctx.color_link = 0x33A0FF;

    render_node(&ctx, doc->root, false);
}