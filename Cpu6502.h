#pragma once
#include <cstdint>
#include "Flags.h"
#include "AddressingMode.h"
#include "Bus.h"

using byte = uint8_t;
using memAddress = uint16_t;

class Cpu6502 {
public:

	Cpu6502() {
		bus = nullptr;
	}

	Cpu6502(Bus* n) {
		bus = n;
	}
	~Cpu6502() = default;

	// Registers
	byte  A = 0x00;   // Accumulator Register
	byte  X = 0x00;   // X Register
	byte  Y = 0x00;   // Y Register
	byte  SP = 0x00;  // Stack Pointer
	memAddress PC = 0x0000; // Program Counter Register
	byte  status = 0x00; // Status Register

	// General
	class Bus* bus = nullptr;

	void reset();

	void executeInterrupt();
	void interrupt(); // Maskable Interrupt
	void nonMaskableInterrupt();

	// Execution
	void clock();
	bool instructionComplete();

	void connectBus(class Bus* busPtr) {
		bus = busPtr;
	}

	// helpers
	void updateZeroAndNegativeFlags(bool zeroCondition, bool negativeCondition);
	void checkPageCrossing();
	void CompareLogic(uint16_t registerValue);

	// addressing modes
	bool IMP(); //Implicit (with Accumulator included)
	bool IMM(); //Immediate
	bool ZP0(); //Zero Page
	bool ZPX(); //Zero Page,X
	bool ZPY(); //Zero Page,Y
	bool REL(); //Relative
	bool ABS(); //Absolute
	bool ABX(); //Absolute,X
	bool ABY(); //Absolute,Y
	bool IND(); //Indirect
	bool IZX(); //Indexed Indirect
	bool IZY(); //Indirect Indexed


	// instructions
	bool ADC();
	bool AND();
	bool ASL();
	bool BCC();
	bool BCS();
	bool BEQ();
	bool BIT();
	bool BMI();
	bool BNE();
	bool BPL();
	bool BRK();
	bool BVC();
	bool BVS();
	bool CLC();

	bool CLD();
	bool CLI();
	bool CLV();
	bool CMP();
	bool CPX();
	bool CPY();
	bool DEC();
	bool DEX();
	bool DEY();
	bool EOR();
	bool INC();
	bool INX();
	bool INY();
	bool JMP();

	bool JSR();
	bool LDA();
	bool LDX();
	bool LDY();
	bool LSR();
	bool NOP();
	bool ORA();
	bool PHA();
	bool PHP();
	bool PLA();
	bool PLP();
	bool ROL();
	bool ROR();
	bool RTI();

	bool RTS();
	bool SBC();
	bool SEC();
	bool SED();
	bool SEI();
	bool STA();
	bool STX();
	bool STY();
	bool TAX();
	bool TAY();
	bool TSX();
	bool TXA();
	bool TXS();
	bool TYA();

	bool XXX(); // Illegal/Unknown Instruction

private:
	byte read(memAddress addr);
	void write(memAddress addr, byte data);

	bool getFlag(Flags flag);
	void setFlag(uint8_t& status, Flags flag); // Set flag
	void clearFlag(uint8_t& status, Flags flag); // Clear flag
	void updateFlag(bool condition, Flags flag); // Set or clear flag based on condition

	// Map addressing mode function pointer to AddressingMode enum
	AddressingMode mapAddressMode(bool (Cpu6502::* addrFn)());

	// Internal helper variables
	byte currentByte = 0x00; // Current data byte 
	byte opcode = 0x00; // Current opcode byte
	AddressingMode currentAddressingMode = AddressingMode::IMP; // Current addressing mode 
	uint8_t cycles = 0;
	memAddress currentAddress = 0x0000;
	memAddress relativeAddress = 0x00;
	bool readFlag = false;
	byte tempByte = 0x00;
	uint16_t tempWord = 0x0000;

};
