#pragma once
#include <ui/html/html_dom.h>

// Build layout boxes for subtree.
// viewport_x/y: drawing origin (content area start)
// viewport_w/h: available area (client/content size)
void html_layout_build(html_node_t* root, int viewport_x, int viewport_y, int viewport_w, int viewport_h);