#include <ui/controls/control.h>
#include <lib/string.h>

void ui_control_init(ui_control_t* c, int id, ui_point_t loc, ui_size_t size, const ui_control_vtbl_t* vtbl) {
    c->id = id;
    memset(c->name, 0, sizeof(c->name));

    c->location = loc;
    c->size = size;

    c->visible = true;
    c->enabled = true;

    c->hovered = false;
    c->pressed = false;

    c->parent = 0;
    c->first_child = 0;
    c->next_sibling = 0;

    c->vtbl = vtbl;
}

bool ui_control_contains(const ui_control_t* c, int px, int py) {
    if (!c) return false;

    int x = c->location.x;
    int y = c->location.y;
    int w = c->size.w;
    int h = c->size.h;

    // w/h 0 ise (label gibi) hit-test olmasın
    if (w <= 0 || h <= 0) return false;

    return (px >= x && py >= y && px < x + w && py < y + h);
}

void ui_control_add_child(ui_control_t* parent, ui_control_t* child) {
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

void ui_control_set_name(ui_control_t* c, const char* name) {
    if (!c) return;

    memset(c->name, 0, sizeof(c->name));
    if (name) {
        strncpy(c->name, name, sizeof(c->name) - 1);
    }
}

ui_control_t* ui_find_control_by_name(ui_control_t* root, const char* name) {
    if (!root || !name) return 0;

    if (root->name[0] && strcmp(root->name, name) == 0) {
        return root;
    }

    ui_control_t* child = root->first_child;
    while (child) {
        ui_control_t* found = ui_find_control_by_name(child, name);
        if (found) return found;
        child = child->next_sibling;
    }

    return 0;
}