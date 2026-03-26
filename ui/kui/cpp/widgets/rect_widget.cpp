#include <ui/kui/cpp/widgets/rect_widget.hpp>
#include <ui/kui/cpp/graphics.hpp>

namespace kui {

void RectWidget::draw(Graphics& gfx) {
    if (!visible) return;

    if (radius > 0) {
        gfx.fillRoundRect(x, y, width, height, radius, color);
    } else {
        gfx.fillRect(x, y, width, height, color);
    }
}

} // namespace kui