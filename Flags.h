#pragma once
#include <cstdint>

enum class Flags : uint8_t
{
	C = (1 << 0),	// Carry Bit
	Z = (1 << 1),	// Zero
	I = (1 << 2),	// Interrupts Toggle
	D = (1 << 3),	// Decimal 
	B = (1 << 4),	// Break
	U = (1 << 5),	// Unused
	V = (1 << 6),	// Overflow
	N = (1 << 7),	// Negative
};
