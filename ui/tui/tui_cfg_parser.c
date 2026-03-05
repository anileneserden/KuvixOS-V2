#include <ui/tui/tui_cfg_parser.h>
#include <ui/tui/tui.h>

#include <lib/string.h>
#include <stdint.h>

static char* trim(char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int tui_cfg_parse(const char* text)
{
    if (!text) return 0;

    const char* p = text;

    while (*p) {

        char line[256];
        int i = 0;

        while (*p && *p != '\n' && i < 255) {
            line[i++] = *p++;
        }

        if (*p == '\n') p++;

        line[i] = 0;

        char* l = trim(line);

        if (l[0] == 0) continue;
        if (l[0] == '#') continue;

        // title
        if (!strncmp(l, "title=", 6)) {

            char* title = l + 6;
            tui_set_title(title);
            continue;
        }

        // item
        if (!strncmp(l, "item=", 5)) {

            char* v = l + 5;

            char* sep = strchr(v, '|');
            if (!sep) continue;

            *sep = 0;

            char* label = v;
            char* action = sep + 1;

            tui_add_item(label, action);

            continue;
        }
    }

    return 1;
}