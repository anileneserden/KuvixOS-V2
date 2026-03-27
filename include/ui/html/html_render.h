#pragma once
#include <stdint.h>
#include <ui/html/html_dom.h>

void html_render_doc(const html_doc_t* doc, int x, int y, int w);
void html_render_node(const html_node_t* node, int x, int y, int w);