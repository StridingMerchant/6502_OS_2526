#pragma once
#include <array>
#include <cstdint>

class RAM {
public:
    static constexpr size_t SIZE = 64 * 1024;

    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t data);

private:
    std::array<uint8_t, SIZE> memory{};
};
