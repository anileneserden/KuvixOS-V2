#include <File.hpp>

#include <kuvixos.h>

bool File::Read(const char* path, char* out, uint32_t cap, uint32_t* outLen) {
    uint32_t readLen = 0;

    if (!path || !out || cap == 0) {
        if (outLen) *outLen = 0;
        return false;
    }

    if (!kvx_file_read_all(path, out, cap, &readLen)) {
        if (outLen) *outLen = 0;
        return false;
    }

    if (outLen) *outLen = readLen;
    return true;
}