#pragma once
#include "Bus.h"
#include "Ram.h"

class FlatBus : public Bus {
public:
    FlatBus();

    uint8_t read(uint16_t addr) override;
    void write(uint16_t addr, uint8_t data) override;

private:
    RAM ram;
};
