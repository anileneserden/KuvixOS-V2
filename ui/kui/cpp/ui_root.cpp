#include <ui/kui/cpp/ui_root.hpp>
#include <ui/kui/cpp/widgets/panel_widget.hpp>
#include <lib/string.h>

namespace kui {

UIRoot::UIRoot() {
    m_root = new PanelWidget();

    m_root->id[0] = 'r';
    m_root->id[1] = 'o';
    m_root->id[2] = 'o';
    m_root->id[3] = 't';
    m_root->id[4] = '\0';
}

UIRoot::~UIRoot() {
    delete m_root;
}

PanelWidget* UIRoot::root() {
    return m_root;
}

void UIRoot::draw(Graphics& gfx) {
    if (m_root) {
        m_root->draw(gfx);
    }
}

Widget* UIRoot::findById(const char* id) {
    if (!id) return 0;
    return findRecursive(m_root, id);
}

Widget* UIRoot::findRecursive(Widget* node, const char* id) {
    if (!node || !id) return 0;

    if (strcmp(node->id, id) == 0) {
        return node;
    }

    if (node->kind() != WidgetKind::Panel) {
        return 0;
    }

    PanelWidget* panel = static_cast<PanelWidget*>(node);

    for (int i = 0; i < panel->childCount; i++) {
        Widget* child = panel->children[i];
        Widget* found = findRecursive(child, id);
        if (found) return found;
    }

    return 0;
}

} // namespace kui