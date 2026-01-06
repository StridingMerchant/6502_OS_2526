#pragma once
#include <array>
#include "AddressingMode.h"

class Cpu6502;

struct Opcode6502 {
    const char* name;
    uint8_t cycles;
    bool (Cpu6502::* operate)();
    bool (Cpu6502::* addrmode)();
};

extern const std::array<Opcode6502, 256> OPCODES_6502;
