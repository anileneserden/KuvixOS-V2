#pragma once
#include <stdint.h>
#include <ui/kui/cpp/widget.hpp>

namespace kui {

class LabelWidget : public Widget {
public:
    char text[128] = {0};
    uint32_t color = 0x00FFFFFF;

    WidgetKind kind() const override { return WidgetKind::Label; }
    void draw(Graphics& gfx) override;
};

} // namespace kui