#pragma once

#include <sdk/kef/app.hpp>

namespace kef {

class Console : public App {
public:
    AppKind Kind() const override {
        return AppKind::Console;
    }

    virtual int Main() = 0;

protected:
    void Print(const char* text);
    void PrintLine(const char* text);
    void Exit(int code = 0);

    int ArgCount() const;
    const char* ArgAt(int index) const;
};

} // namespace kef