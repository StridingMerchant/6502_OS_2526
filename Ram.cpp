#include "Ram.h"

uint8_t RAM::read(uint16_t addr) const {
    return memory[addr];
}

void RAM::write(uint16_t addr, uint8_t data) {
    memory[addr] = data;
}
