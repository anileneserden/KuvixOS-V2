#pragma once

#include "UIElement.hpp"

class ComboBox : public UIElement {
public:
    ComboBox(kef_minimal_state_t* st = nullptr, kef_widget_t* w = nullptr)
        : UIElement(st, w) {}

    int getSelectedIndex() const {
        if (!isValid()) return -1;
        return widget->combo_selected;
    }

    void setSelectedIndex(int index) {
        if (!isValid()) return;
        if (index < 0 || index >= widget->combo_item_count) return;
        widget->combo_selected = index;
    }

    const char* getSelectedText() const {
        if (!isValid()) return "";
        int idx = widget->combo_selected;
        if (idx < 0 || idx >= widget->combo_item_count) return "";
        return widget->combo_items[idx];
    }

    int getItemCount() const {
        if (!isValid()) return 0;
        return widget->combo_item_count;
    }

    const char* getItem(int index) const {
        if (!isValid()) return "";
        if (index < 0 || index >= widget->combo_item_count) return "";
        return widget->combo_items[index];
    }
};