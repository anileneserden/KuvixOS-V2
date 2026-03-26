#include <ui/kui/cpp/widgets/panel_widget.hpp>
#include <ui/kui/cpp/graphics.hpp>

namespace kui {

void PanelWidget::addChild(Widget* child) {
    if (!child) return;
    if (childCount >= MAX_CHILDREN) return;

    children[childCount++] = child;
}

void PanelWidget::draw(Graphics& gfx) {
    if (!visible) return;

    if (radius > 0) {
        gfx.fillRoundRect(x, y, width, height, radius, backgroundColor);
    } else {
        gfx.fillRect(x, y, width, height, backgroundColor);
    }

    for (int i = 0; i < childCount; i++) {
        if (children[i]) {
            children[i]->draw(gfx);
        }
    }
}

} // namespace kui