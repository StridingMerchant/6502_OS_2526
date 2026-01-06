#pragma once
#include <cstdint>

// 6502 addressing modes
enum class AddressingMode : uint8_t {
    IMP, // Implicit/Accumulator
    IMM, // Immediate
    ZP0, // Zero Page
    ZPX, // Zero Page,X
    ZPY, // Zero Page,Y
    REL, // Relative
    ABS, // Absolute
    ABX, // Absolute,X
    ABY, // Absolute,Y
    IND, // Indirect
    IZX, // Indexed Indirect (X)
    IZY, // Indirect Indexed (Y)
};
