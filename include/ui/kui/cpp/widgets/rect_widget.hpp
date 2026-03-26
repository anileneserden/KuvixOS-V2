#pragma once
#include <stdint.h>
#include <ui/kui/cpp/widget.hpp>

namespace kui {

class RectWidget : public Widget {
public:
    uint32_t color = 0x00FFFFFF;
    int radius = 0;

    WidgetKind kind() const override { return WidgetKind::Rect; }
    void draw(Graphics& gfx) override;
};

} // namespace kui