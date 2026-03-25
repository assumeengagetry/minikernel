#include "vga.hpp"

namespace vga {
    static constexpr size_t kCols = 80;
    static constexpr size_t kRows = 25;
    static constexpr uint8_t kDefaultAttr = 0x07;
    static volatile uint16_t* const buffer =
        reinterpret_cast<volatile uint16_t*>(0xB8000);

    static inline uint16_t make_entry(char c, uint8_t attr) {
        return (uint16_t(attr) << 8) | uint8_t(c);
    }

    void clear() {
        for (size_t i = 0; i < kCols * kRows; ++i) {
            buffer[i] = make_entry(' ', kDefaultAttr);
        }
    }

    void write_string(const char* s, size_t row, size_t col, uint8_t attr) {
        size_t index = row * kCols + col;
        while (*s != '\0' && index < kCols * kRows) {
            buffer[index++] = make_entry(*s++, attr);
        }
    }
}
