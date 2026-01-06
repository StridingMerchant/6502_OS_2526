#include "FlatBus.h"

FlatBus::FlatBus() = default;

uint8_t FlatBus::read(uint16_t addr) {
    return ram.read(addr);
}

void FlatBus::write(uint16_t addr, uint8_t data) {
    ram.write(addr, data);
}