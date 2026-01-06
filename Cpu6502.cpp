#include "Cpu6502.h"
#include "Opcodes.h"
#include "Bus.h"
#include "AddressingMode.h"
#include <iostream>

constexpr uint16_t HIGH_BYTE_MASK = 0xFF00;
constexpr uint16_t LOW_BYTE_MASK = 0x00FF;
constexpr uint16_t ZERO_PAGE_BOUNDARY = 0x00FF;
constexpr uint16_t STACK_BASE_ADDRESS = 0x0100;
constexpr uint8_t SIGN_BIT_MASK = 0x80;



byte Cpu6502::read(memAddress addr)
{
	if( currentAddressingMode == AddressingMode::IMP )
		return currentByte;
	return bus->read(addr);
}

void Cpu6502::write(memAddress addr, byte data)
{
	bus->write(addr, data);
}

bool Cpu6502::getFlag(Flags flag)
{
	return (status & static_cast<uint8_t>(flag)) != 0;
}

void Cpu6502::setFlag(uint8_t& status, Flags flag)
{
	status |= static_cast<uint8_t>(flag);
}

void Cpu6502::clearFlag(uint8_t& status, Flags flag)
{
	status &= ~static_cast<uint8_t>(flag);
}

void Cpu6502::updateFlag(bool condition, Flags flag)
{
	if (condition)
		setFlag(status, flag);
	else
		clearFlag(status, flag);
}

void Cpu6502::reset()
{
	// Set Program Counter to the address stored at the Reset vector (0xFFFC and 0xFFFD)
	PC = static_cast<memAddress>(bus->read(0xFFFC)) | (static_cast<memAddress>(bus->read(0xFFFD)) << 8);

	// Reset registers
	A = 0;
	X = 0;
	Y = 0;

	SP = 0xFD; // Stack Pointer starts at 0xFD after reset

	status = 0x00 | static_cast<uint8_t>(Flags::U); // Set unused flag

	currentAddress = 0;
	currentByte = 0;
	relativeAddress = 0;

	cycles = 8;
}

void Cpu6502::executeInterrupt() {
	// Push PC and Status onto the stack
	write(static_cast<memAddress>(STACK_BASE_ADDRESS + SP--), static_cast<byte>((PC >> 8) & LOW_BYTE_MASK)); // Push high byte of PC
	write(static_cast<memAddress>(STACK_BASE_ADDRESS + SP--), static_cast<byte>(PC & LOW_BYTE_MASK));        // Push low byte of PC

	// Clear Break flag and set Unused flag before pushing status
	updateFlag(false, Flags::B);
	updateFlag(true, Flags::U);

	write(static_cast<memAddress>(STACK_BASE_ADDRESS + SP--), status); // Push status register

	// Set Interrupt Disable flag
	updateFlag(true, Flags::I);

	// Set PC to the address stored at the IRQ vector (0xFFFE and 0xFFFF)
	PC = static_cast<memAddress>(bus->read(0xFFFE)) | (static_cast<memAddress>(bus->read(0xFFFF)) << 8);
}

void Cpu6502::interrupt()
{
	if( !getFlag(Flags::I) ) // Only process IRQ if Interrupt Disable flag is clear
	{
		executeInterrupt();
		cycles = 7; // IRQ takes 7 cycles
	}
}

void Cpu6502::nonMaskableInterrupt()
{
	executeInterrupt();
	cycles = 8; // NMI takes 8 cycles
}

AddressingMode Cpu6502::mapAddressMode(bool (Cpu6502::* addrFn)())
{
	if (addrFn == &Cpu6502::IMP) return AddressingMode::IMP;
	if (addrFn == &Cpu6502::IMM) return AddressingMode::IMM;
	if (addrFn == &Cpu6502::ZP0) return AddressingMode::ZP0;
	if (addrFn == &Cpu6502::ZPX) return AddressingMode::ZPX;
	if (addrFn == &Cpu6502::ZPY) return AddressingMode::ZPY;
	if (addrFn == &Cpu6502::REL) return AddressingMode::REL;
	if (addrFn == &Cpu6502::ABS) return AddressingMode::ABS;
	if (addrFn == &Cpu6502::ABX) return AddressingMode::ABX;
	if (addrFn == &Cpu6502::ABY) return AddressingMode::ABY;
	if (addrFn == &Cpu6502::IND) return AddressingMode::IND;
	if (addrFn == &Cpu6502::IZX) return AddressingMode::IZX;
	if (addrFn == &Cpu6502::IZY) return AddressingMode::IZY;
	// Default fallback
	return AddressingMode::IMP;
}

void Cpu6502::clock()
{
	if (cycles == 0)
	{
		// Fetch opcode
		opcode = bus->read(PC);
		PC++;

		// Set unused flag
		updateFlag(true, Flags::U);

		// Get the corresponding instruction from the opcode table
		const Opcode6502& instruction = OPCODES_6502[opcode];

		// Calculate total cycles
		cycles = instruction.cycles;

		// Set the current addressing mode and operation
		currentAddressingMode = mapAddressMode(instruction.addrmode);

		// Execute addressing mode
		bool additionalCycleAddressingMode = (this->*instruction.addrmode)();

		// Execute operation
		(this->*instruction.operate)();


		// Check if the instruction is a store instruction - these do not get additional cycles for page crossing
		bool isStoreInstruction = (instruction.operate == &Cpu6502::STA) ||
			(instruction.operate == &Cpu6502::STX) ||
			(instruction.operate == &Cpu6502::STY);

		if (additionalCycleAddressingMode && !isStoreInstruction)
			cycles++;
	}
	cycles--;
}

bool Cpu6502::instructionComplete()
{
	return cycles == 0;
}

// === Helpers ===

// Helper function to get zero page addresses correctly - typecast to uint16_t to ensure proper addressing to 0x00 - 0xFF 
static constexpr memAddress zeroPage(byte addr)
{
	return static_cast<memAddress>(addr & LOW_BYTE_MASK);
}

// Helper function to get absolute addresses from low and high bytes
static constexpr memAddress getAbsolute(byte lowByte, byte highByte)
{
	return static_cast<memAddress>(highByte) << 8 | lowByte;
}

// Helper function to update Zero and Negative flags based on conditions
void Cpu6502::updateZeroAndNegativeFlags(bool zeroCondition, bool negativeCondition)
{
	// Set or clear Zero Flag
	updateFlag(zeroCondition, Flags::Z);
	// Set or clear Negative Flag
	updateFlag(negativeCondition, Flags::N);
}

// Helper function to check for page crossing and add cycle if needed
void Cpu6502::checkPageCrossing()
{
	if ((currentAddress & HIGH_BYTE_MASK) != (PC & HIGH_BYTE_MASK))
		cycles++;
}

// Helper function for comparison logic used in CMP, CPX, CPY instructions
void Cpu6502::CompareLogic(uint16_t registerValue)
{
	uint16_t temp = registerValue - static_cast<uint16_t>(currentByte);
	// Set or clear Carry Flag
	updateFlag(registerValue >= static_cast<uint16_t>(currentByte), Flags::C);
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((temp & LOW_BYTE_MASK) == 0, (temp & SIGN_BIT_MASK) != 0);
}

// === Addressing Modes ===
// All addressing modes and instructions return true if they require an additional cycle

bool Cpu6502::IMP()
{
	// Implicit addressing mode (with Accumulator included) - save accumulator value to currentByte, no additional cycle needed
	currentByte = A;
	return false;
}

bool Cpu6502::IMM()
{
	// Immediate addressing mode - set currentAddress to the next byte after the opcode
	currentAddress = PC++;
	return false;
}

// Zero Page addressing mode - read the zero page address from program counter, increment PC, and set currentAddress
bool Cpu6502::ZP0()
{
	currentAddress = zeroPage(bus->read(PC));
	PC++;
	return false;
}

// Zero Page,X addressing mode - read the zero page address from program counter, add X register offset, set currentAddress, and increment PC
bool Cpu6502::ZPX()
{
	currentAddress = zeroPage(bus->read(PC) + X); // Offset is stored in X register
	PC++;
	return false;
}

// Same as ZPX but with Y register
bool Cpu6502::ZPY()
{
	currentAddress = zeroPage(bus->read(PC) + Y); // Offset is stored in Y register
	PC++;
	return false;
}

// Relative addressing mode
bool Cpu6502::REL()
{
	relativeAddress = bus->read(PC);
	PC++;
	// If the relative address is negative, sign-extend it to 16 bits
	if (relativeAddress & SIGN_BIT_MASK)
		relativeAddress |= HIGH_BYTE_MASK;
	return false;
}

// Absolute addressing mode
bool Cpu6502::ABS()
{
	currentAddress = getAbsolute(bus->read(PC), bus->read(PC + 1));
	PC += 2;
	return false;
}

// Absolute,X addressing mode
bool Cpu6502::ABX()
{
	memAddress base = getAbsolute(bus->read(PC), bus->read(PC + 1));
	PC += 2;
	currentAddress = base + X;

	// Page crossing occurs if high byte changed after indexing
	if ((base & HIGH_BYTE_MASK) != (currentAddress & HIGH_BYTE_MASK))
		return true;

	return false;
}

// Absolute,Y addressing mode - similar to ABX but with Y register
bool Cpu6502::ABY()
{
	memAddress base = getAbsolute(bus->read(PC), bus->read(PC + 1));
	PC += 2;
	currentAddress = base + Y;

	// Page crossing occurs if high byte changed after indexing
	if ((base & HIGH_BYTE_MASK) != (currentAddress & HIGH_BYTE_MASK))
		return true;

	return false;
}

// Indirect addressing mode - has a hardware bug when the low byte is 0xFF
bool Cpu6502::IND()
{
	memAddress pointer = getAbsolute(bus->read(PC), bus->read(PC + 1));
	PC += 2;
	// Simulate the hardware bug
	if ((pointer & LOW_BYTE_MASK) == ZERO_PAGE_BOUNDARY)
	{
		currentAddress = getAbsolute(bus->read(pointer), bus->read(pointer & HIGH_BYTE_MASK)); // Wrap around to the beginning of the page
	}
	else
	{
		currentAddress = getAbsolute(bus->read(pointer), bus->read(pointer + 1));
	}
	return false;
}

// Indexed Indirect addressing mode - using X register
bool Cpu6502::IZX()
{
	byte t = bus->read(PC);
	PC++;

	t += X; // Add X register to the zero page address
	currentAddress = getAbsolute(bus->read(zeroPage(t)), bus->read(zeroPage(t + 1)));
	return false;
}

// Indirect Indexed addressing mode - similar to IZX but with Y register
bool Cpu6502::IZY()
{
	byte t = bus->read(PC);
	PC++;

	memAddress base = getAbsolute(bus->read(zeroPage(t)), bus->read(zeroPage(t + 1))); // IZY forms address before adding Y

	currentAddress = static_cast<memAddress>(base + Y);
	if ((base & HIGH_BYTE_MASK) != (currentAddress & HIGH_BYTE_MASK))
		return true;
	return false;
}

// === Instructions ===

// ADC - Add with Carry
bool Cpu6502::ADC()
{
	currentByte = read(currentAddress);

	tempWord = static_cast<uint16_t>(A) + static_cast<uint16_t>(currentByte) + static_cast<uint16_t>(getFlag(Flags::C));

	// Set or clear Carry Flag
	updateFlag(tempWord > LOW_BYTE_MASK, Flags::C);

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((tempWord & LOW_BYTE_MASK) == 0, (tempWord & SIGN_BIT_MASK) != 0);

	// Set or clear Overflow Flag
	updateFlag(((~(static_cast<uint16_t>(A) ^ static_cast<uint16_t>(currentByte))) & (static_cast<uint16_t>(A) ^ tempWord) & SIGN_BIT_MASK) != 0, Flags::V);

	A = static_cast<byte>(tempWord & LOW_BYTE_MASK);

	// ADC may require an additional cycle if page boundary is crossed
	return false;
}

// AND - Logical AND between Accumulator and memory
bool Cpu6502::AND()
{
	currentByte = read(currentAddress);
	A = A & currentByte;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);

	// AND may require an additional cycle if page boundary is crossed
	return false;
}

// ASL - Arithmetic Shift Left
bool Cpu6502::ASL()
{
	currentByte = read(currentAddress);

	// Set or clear Carry Flag based on bit 7
	updateFlag((currentByte & SIGN_BIT_MASK) != 0, Flags::C);
	currentByte <<= 1;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(currentByte == 0, (currentByte & SIGN_BIT_MASK) != 0);

	if (currentAddressingMode == AddressingMode::IMP)
	{
		// Accumulator form: write back to A
		A = static_cast<byte>(currentByte);
	}
	else
	{
		// Memory form: write back to addressed memory
		write(currentAddress, static_cast<byte>(currentByte));
	}

	return false;
}

// BCC - Branch if Carry Clear
bool Cpu6502::BCC()
{
	if(!getFlag(Flags::C))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}

	return false;
}

// BCS - Branch if Carry Set
bool Cpu6502::BCS()
{
	if(getFlag(Flags::C))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BEQ - Branch if Equal (Zero Flag Set)
bool Cpu6502::BEQ()
{
	if(getFlag(Flags::Z))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BIT - Bit Test
bool Cpu6502::BIT()
{
	currentByte = read(currentAddress);

	// Set or clear Zero and Negative Flags based on AND result and bit 7 of currentByte
	updateZeroAndNegativeFlags((A & currentByte) == 0, (currentByte & (1 << 7)) != 0);

	// Set or clear Overflow Flag based on bit 6 of currentByte
	updateFlag((currentByte & (1 << 6)) != 0, Flags::V);

	return false;
}

// BMI - Branch if Minus (Negative Flag Set)
bool Cpu6502::BMI()
{
	if(getFlag(Flags::N))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BNE - Branch if Not Equal (Zero Flag Clear)
bool Cpu6502::BNE()
{
	if(!getFlag(Flags::Z))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BPL - Branch if Positive (Negative Flag Clear)
bool Cpu6502::BPL()
{
	if(!getFlag(Flags::N))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BRK - Force Interrupt
bool Cpu6502::BRK()
{
	PC++;
	setFlag(status, Flags::I);
	write(0x0100 + SP--, (PC >> 8) & LOW_BYTE_MASK);	// Push high byte of PC
	write(0x0100 + SP--, PC & LOW_BYTE_MASK);			// Push low byte of PC
	setFlag(status, Flags::B);							// Set Break Flag
	write(0x0100 + SP--, status);						// Push status register
	clearFlag(status, Flags::B);						// Clear Break Flag

	setFlag(status, Flags::I);						// Set Interrupt Disable Flag

	PC = getAbsolute(bus->read(0xFFFE), bus->read(0xFFFF));		// Set PC to IRQ/BRK vector address
	return false;
}

// BVC - Branch if Overflow Clear
bool Cpu6502::BVC()
{
	if(!getFlag(Flags::V))
	{
		cycles++;
		currentAddress = PC + relativeAddress;

		checkPageCrossing();

		PC = currentAddress;
	}
	return false;
}

// BVS - Branch if Overflow Set
bool Cpu6502::BVS()
{
	if(getFlag(Flags::V))
	{
		cycles++;
		currentAddress = PC + relativeAddress;
		
		checkPageCrossing();
	
		PC = currentAddress;
	}
	return false;
}

// CLC - Clear Carry Flag
bool Cpu6502::CLC()
{
	clearFlag(status, Flags::C);
	return false;
}

// CLD - Clear Decimal Mode - essentially unused in NES emulation, but implemented for completeness
bool Cpu6502::CLD()
{
	clearFlag(status, Flags::D);
	return false;
}

// CLI - Clear Interrupt Disable
bool Cpu6502::CLI()
{
	clearFlag(status, Flags::I);
	return false;
}

// CLV - Clear Overflow Flag
bool Cpu6502::CLV()
{
	clearFlag(status, Flags::V);
	return false;
}

// CMP - Compare Accumulator
bool Cpu6502::CMP()
{
	currentByte = read(currentAddress);
	
	CompareLogic(static_cast<uint16_t>(A));

	return false;
}

// CPX - Compare X Register
bool Cpu6502::CPX()
{
	currentByte = read(currentAddress);

	CompareLogic(static_cast<uint16_t>(X));

	return false;
}

// CPY - Compare Y Register
bool Cpu6502::CPY()
{
	currentByte = read(currentAddress);

	CompareLogic(static_cast<uint16_t>(Y));

	return false;
}

// DEC - Decrement Memory value
bool Cpu6502::DEC()
{
	currentByte = read(currentAddress);

	tempByte = currentByte - 1;

	write(currentAddress, tempByte & LOW_BYTE_MASK);
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((tempByte & LOW_BYTE_MASK) == 0, (tempByte & SIGN_BIT_MASK) != 0);

	return false;
}

// DEX - Decrement X Register
bool Cpu6502::DEX()
{
	X--;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(X == 0, (X & SIGN_BIT_MASK) != 0);

	return false;
}

// DEY - Decrement Y Register
bool Cpu6502::DEY()
{
	Y--;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(Y == 0, (Y & SIGN_BIT_MASK) != 0);

	return false;
}

// EOR - Exclusive OR between Accumulator and memory
bool Cpu6502::EOR()
{
	currentByte = read(currentAddress);

	A = A ^ currentByte;
	
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);

	return false;
}

// INC - Increment Memory value
bool Cpu6502::INC()
{
	currentByte = read(currentAddress);

	tempByte = currentByte + 1;
	write(currentAddress, tempByte & LOW_BYTE_MASK);
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((tempByte & LOW_BYTE_MASK) == 0, (tempByte & SIGN_BIT_MASK) != 0);

	return false;
}

// INX - Increment X Register
bool Cpu6502::INX()
{
	X++;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(X == 0, (X & SIGN_BIT_MASK) != 0);

	return false;
}

// INY - Increment Y Register
bool Cpu6502::INY()
{
	Y++;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(Y == 0, (Y & SIGN_BIT_MASK) != 0);

	return false;
}

// JMP - Jump to new location in memory
bool Cpu6502::JMP()
{
	PC = currentAddress;
	return false;
}

// JSR - Jump to Subroutine
bool Cpu6502::JSR()
{
	PC--;
	write(0x0100 + SP--, (PC >> 8) & LOW_BYTE_MASK);	// Push high byte of PC
	write(0x0100 + SP--, PC & LOW_BYTE_MASK);			// Push low byte of PC

	PC = currentAddress;

	return false;
}

// LDA - Load Accumulator
bool Cpu6502::LDA()
{
	currentByte = read(currentAddress);
	A = currentByte;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);

	return false;
}

// LDX - Load X Register
bool Cpu6502::LDX()
{
	currentByte = read(currentAddress);
	X = currentByte;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(X == 0, (X & SIGN_BIT_MASK) != 0);

	return false;
}

// LDY - Load Y Register
bool Cpu6502::LDY()
{
	currentByte = read(currentAddress);
	Y = currentByte;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(Y == 0, (Y & SIGN_BIT_MASK) != 0);

	return false;
}

// LSR - Logical Shift Right
bool Cpu6502::LSR()
{
	currentByte = read(currentAddress);
	// Set or clear Carry Flag based on bit 0
	updateFlag((currentByte & 0x01) != 0, Flags::C);

	tempByte = currentByte >> 1;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((tempByte & LOW_BYTE_MASK) == 0, (tempByte & SIGN_BIT_MASK) != 0);

	// Write result back to Accumulator or memory
	if (currentAddressingMode == AddressingMode::IMP)
		A = static_cast<byte>(tempByte & LOW_BYTE_MASK);
	else
		write(currentAddress, static_cast<byte>(tempByte & LOW_BYTE_MASK));

	return false;
}

// NOP - No Operation - there are some unofficial NOPs that take additional cycles or have different addressing modes, but this is the standard one
bool Cpu6502::NOP()
{
	return false;
}

// ORA - Logical Inclusive OR between Accumulator and memory
bool Cpu6502::ORA()
{
	currentByte = read(currentAddress);
	
	A = A | currentByte;

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);

	return false;
}

// PHA - Push Accumulator onto Stack
bool Cpu6502::PHA()
{
	write(STACK_BASE_ADDRESS + SP--, A);
	return false;
}

// PHP - Push Processor Status onto Stack
bool Cpu6502::PHP()
{
	write(STACK_BASE_ADDRESS + SP--, status | static_cast<uint8_t>(Flags::B) | static_cast<uint8_t>(Flags::U));
	return false;
}

// PLA - Pop Accumulator from Stack
bool Cpu6502::PLA()
{
	SP++;
	A = bus->read(STACK_BASE_ADDRESS + SP);
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);

	return false;
}

// PLP - Pop Processor Status from Stack
bool Cpu6502::PLP()
{
	SP++;
	status = bus->read(STACK_BASE_ADDRESS + SP);

	setFlag(status, Flags::U); // Unused flag is always set

	return false;
}

// ROL - Rotate Left
bool Cpu6502::ROL()
{
	// Use A directly for accumulator form, memory otherwise
	byte value = (currentAddressingMode == AddressingMode::IMP) ? A : read(currentAddress);

	const bool oldCarry = getFlag(Flags::C);
	const bool newCarry = (value & SIGN_BIT_MASK) != 0;

	value = static_cast<byte>((value << 1) | (oldCarry ? 1 : 0));

	updateFlag(newCarry, Flags::C);
	updateZeroAndNegativeFlags(value == 0, (value & SIGN_BIT_MASK) != 0);

	if (currentAddressingMode == AddressingMode::IMP)
		A = value;
	else
		write(currentAddress, value);

	return false;
}

// ROR - Rotate Right
bool Cpu6502::ROR()
{
	// Use A directly for accumulator form, memory otherwise
	byte value = (currentAddressingMode == AddressingMode::IMP) ? A : read(currentAddress);

	const bool oldCarry = getFlag(Flags::C);
	const bool newCarry = (value & 0x01) != 0;

	value = static_cast<byte>((value >> 1) | (oldCarry ? 0x80 : 0x00));

	updateFlag(newCarry, Flags::C);
	updateZeroAndNegativeFlags(value == 0, (value & SIGN_BIT_MASK) != 0);

	if (currentAddressingMode == AddressingMode::IMP)
		A = value;
	else
		write(currentAddress, value);

	return false;
}

// RTI - Return from Interrupt
bool Cpu6502::RTI()
{
	SP++;
	status = bus->read(STACK_BASE_ADDRESS + SP);
	clearFlag(status, Flags::B); // Clear Break Flag
	setFlag(status, Flags::U); // Set Unused Flag

	SP++;
	PC = getAbsolute(bus->read(STACK_BASE_ADDRESS + SP), bus->read(STACK_BASE_ADDRESS + SP + 1));
	SP++;

	return false;
}

// RTS - Return from Subroutine
bool Cpu6502::RTS()
{
	SP++;
	PC = getAbsolute(bus->read(STACK_BASE_ADDRESS + SP), bus->read(STACK_BASE_ADDRESS + SP + 1));
	SP++;

	PC++; // Increment PC to point to the next instruction after JSR

	return false;
}

// SBC - Subtract with Carry
bool Cpu6502::SBC()
{
	currentByte = read(currentAddress);

	// Invert currentByte for subtraction
	uint16_t value_inv = static_cast<uint16_t>(currentByte) ^ LOW_BYTE_MASK;

	tempWord = static_cast<uint16_t>(A) + static_cast<uint16_t>(value_inv) + static_cast<uint16_t>(getFlag(Flags::C));

	// Set or clear Carry Flag
	updateFlag(tempWord & HIGH_BYTE_MASK, Flags::C);

	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags((tempWord & LOW_BYTE_MASK) == 0, (tempWord & SIGN_BIT_MASK) != 0);

	// Set or clear Overflow Flag
	updateFlag((((tempWord ^ static_cast<uint16_t>(A)) & (tempWord ^ value_inv)) & SIGN_BIT_MASK) != 0, Flags::V);

	A = static_cast<byte>(tempWord & LOW_BYTE_MASK);

	// SBC may require an additional cycle
	return false;
}


// SEC - Set Carry Flag
bool Cpu6502::SEC()
{
	setFlag(status, Flags::C);
	return false;
}

// SED - Set Decimal Flag - essentially unused in NES emulation, but implemented for completeness
bool Cpu6502::SED()
{
	setFlag(status, Flags::D);
	return false;
}

// SEI - Set Interrupt Disable
bool Cpu6502::SEI()
{
	setFlag(status, Flags::I);
	return false;
}

// STA - Store Accumulator in memory
bool Cpu6502::STA()
{
	write(currentAddress, A);
	return false;
}

// STX - Store X Register in memory
bool Cpu6502::STX()
{
	write(currentAddress, X);
	return false;
}

// STY - Store Y Register in memory
bool Cpu6502::STY()
{
	write(currentAddress, Y);
	return false;
}

// TAX - Transfer Accumulator to X Register
bool Cpu6502::TAX()
{
	X = A;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(X == 0, (X & SIGN_BIT_MASK) != 0);
	return false;
}

// TAY - Transfer Accumulator to Y Register
bool Cpu6502::TAY()
{
	Y = A;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(Y == 0, (Y & SIGN_BIT_MASK) != 0);
	return false;
}

// TSX - Transfer Stack Pointer to X Register
bool Cpu6502::TSX()
{
	X = SP;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(X == 0, (X & SIGN_BIT_MASK) != 0);
	return false;
}

// TXA - Transfer X Register to Accumulator
bool Cpu6502::TXA()
{
	A = X;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);
	return false;
}

// TXS - Transfer X Register to Stack Pointer
bool Cpu6502::TXS()
{
	SP = X;
	return false;
}

// TYA - Transfer Y Register to Accumulator
bool Cpu6502::TYA()
{
	A = Y;
	// Set or clear Zero and Negative Flags
	updateZeroAndNegativeFlags(A == 0, (A & SIGN_BIT_MASK) != 0);
	return false;
}

// XXX - Illegal/Unknown Instruction
bool Cpu6502::XXX()
{
	return false;
}
