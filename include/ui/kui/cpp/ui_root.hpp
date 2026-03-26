#pragma once

namespace kui {

class Graphics;
class Widget;
class PanelWidget;

class UIRoot {
public:
    UIRoot();
    ~UIRoot();

    PanelWidget* root();

    void draw(Graphics& gfx);
    Widget* findById(const char* id);

private:
    PanelWidget* m_root;

    Widget* findRecursive(Widget* node, const char* id);
};

} // namespace kui