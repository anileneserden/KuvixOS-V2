#pragma once
#include <stdint.h>
#include <ui/kui/cpp/widget.hpp>

namespace kui {

class PanelWidget : public Widget {
public:
    static const int MAX_CHILDREN = 64;

    uint32_t backgroundColor = 0x00202020;
    int radius = 0;

    Widget* children[MAX_CHILDREN] = {0};
    int childCount = 0;

    WidgetKind kind() const override { return WidgetKind::Panel; }
    void addChild(Widget* child);
    void draw(Graphics& gfx) override;
};

} // namespace kui