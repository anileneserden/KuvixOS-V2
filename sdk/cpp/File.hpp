#pragma once

#include <stdint.h>

class File {
public:
    static bool Read(const char* path, char* out, uint32_t cap, uint32_t* outLen = 0);
};