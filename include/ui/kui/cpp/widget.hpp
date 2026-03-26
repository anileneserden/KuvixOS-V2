#pragma once

namespace kui {

class Graphics;

enum class WidgetKind {
    Base = 0,
    Rect,
    Label,
    Panel
};

class Widget {
public:
    virtual ~Widget() = default;

    char id[64] = {0};

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool visible = true;

    virtual WidgetKind kind() const { return WidgetKind::Base; }
    virtual void draw(Graphics& gfx) = 0;

    bool contains(int px, int py) const {
        return visible &&
               px >= x &&
               py >= y &&
               px < (x + width) &&
               py < (y + height);
    }
};

} // namespace kuis