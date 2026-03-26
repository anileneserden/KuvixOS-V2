#include <ui/kui/cpp/widgets/label_widget.hpp>
#include <ui/kui/cpp/graphics.hpp>

namespace kui {

void LabelWidget::draw(Graphics& gfx) {
    if (!visible) return;
    gfx.drawTextUtf8(x, y, color, text);
}

} // namespace kui