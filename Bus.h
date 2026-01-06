#pragma once
#include <cstdint>

class Bus {
public:
    virtual ~Bus() = default;

    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t data) = 0;
};
