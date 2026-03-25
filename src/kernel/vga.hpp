#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vga {
    void clear();
    void write_string(const char* s, size_t row, size_t col, uint8_t attr);
}
