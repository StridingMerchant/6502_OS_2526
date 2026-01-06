#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cctype>

#include "Cpu6502.h"
#include "FlatBus.h"
#include "Opcodes.h"

static bool LoadBinaryToBus(Bus& bus, const std::string& path, uint16_t baseAddr)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (buffer.empty()) {
        return false;
    }

    uint16_t addr = baseAddr;
    for (uint8_t b : buffer) {
        bus.write(addr, b);
        addr++;
    }
    return true;
}

static bool DumpMemoryToLog(Bus& bus, const std::string& outPath)
{
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << std::hex << std::uppercase << std::setfill('0');

    for (uint32_t base = 0; base <= 0xFFFF; base += 16) {
        out << std::setw(4) << base << ":";
        for (uint32_t i = 0; i < 16; ++i) {
            uint16_t addr = static_cast<uint16_t>(base + i);
            uint8_t val = bus.read(addr);
            out << " " << std::setw(2) << static_cast<unsigned>(val);
        }
        out << "\n";
    }

    out.flush();
    return true;
}

static void PrintCpuStateLine(const Cpu6502& cpu, Bus& bus, uint64_t cycAtFetch)
{
    const uint16_t pc = cpu.PC;
    const uint8_t op = bus.read(pc);
    const Opcode6502& ins = OPCODES_6502[op];

    const uint8_t b1 = bus.read(static_cast<uint16_t>(pc + 1));
    const uint8_t b2 = bus.read(static_cast<uint16_t>(pc + 2));
    const uint16_t word = static_cast<uint16_t>(b1) | (static_cast<uint16_t>(b2) << 8);

    int byteCount = 1;
    if (ins.addrmode == &Cpu6502::IMM || ins.addrmode == &Cpu6502::ZP0 || ins.addrmode == &Cpu6502::ZPX ||
        ins.addrmode == &Cpu6502::ZPY || ins.addrmode == &Cpu6502::REL || ins.addrmode == &Cpu6502::IZX ||
        ins.addrmode == &Cpu6502::IZY) {
        byteCount = 2;
    } else if (ins.addrmode == &Cpu6502::ABS || ins.addrmode == &Cpu6502::ABX ||
               ins.addrmode == &Cpu6502::ABY || ins.addrmode == &Cpu6502::IND) {
        byteCount = 3;
    } else {
        byteCount = 1;
    }

    auto hex2 = [&](uint8_t v) { std::ostringstream s; s << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<unsigned>(v); return s.str(); };
    auto hex4 = [&](uint16_t v) { std::ostringstream s; s << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << v; return s.str(); };

    std::cout << std::hex << std::uppercase << std::setfill('0')
              << std::setw(4) << pc << "  ";

    std::cout << std::setw(2) << static_cast<unsigned>(op) << " ";
    if (byteCount >= 2) {
        std::cout << std::setw(2) << static_cast<unsigned>(b1) << " ";
    } else {
        std::cout << "   ";
    }
    if (byteCount >= 3) {
        std::cout << std::setw(2) << static_cast<unsigned>(b2) << " ";
    } else {
        std::cout << "   ";
    }

    std::ostringstream mnem;
    mnem << ins.name << " ";
    if (ins.addrmode == &Cpu6502::IMM) {
        mnem << "#$" << hex2(b1);
    } else if (ins.addrmode == &Cpu6502::ZP0) {
        mnem << "$" << hex2(b1);
    } else if (ins.addrmode == &Cpu6502::ZPX) {
        mnem << "$" << hex2(b1) << ",X";
    } else if (ins.addrmode == &Cpu6502::ZPY) {
        mnem << "$" << hex2(b1) << ",Y";
    } else if (ins.addrmode == &Cpu6502::ABS) {
        mnem << "$" << hex4(word);
    } else if (ins.addrmode == &Cpu6502::ABX) {
        mnem << "$" << hex4(word) << ",X";
    } else if (ins.addrmode == &Cpu6502::ABY) {
        mnem << "$" << hex4(word) << ",Y";
    } else if (ins.addrmode == &Cpu6502::IND) {
        mnem << "($" << hex4(word) << ")";
    } else if (ins.addrmode == &Cpu6502::IZX) {
        mnem << "($" << hex2(b1) << ",X)";
    } else if (ins.addrmode == &Cpu6502::IZY) {
        mnem << "($" << hex2(b1) << "),Y";
    } else if (ins.addrmode == &Cpu6502::REL) {
        int8_t rel = static_cast<int8_t>(b1);
        uint16_t target = static_cast<uint16_t>(pc + 2 + rel);
        mnem << "$" << hex4(target);
    } 

    std::string m = mnem.str();
    if (m.size() < 28) m.append(28 - m.size(), ' ');
    std::cout << m;

    std::cout << "A:" << std::setw(2) << static_cast<unsigned>(cpu.A)
              << " X:" << std::setw(2) << static_cast<unsigned>(cpu.X)
              << " Y:" << std::setw(2) << static_cast<unsigned>(cpu.Y)
              << " P:" << std::setw(2) << static_cast<unsigned>(cpu.status)
              << " SP:" << std::setw(2) << static_cast<unsigned>(cpu.SP)
              << " CYC:" << std::dec << static_cast<unsigned long long>(cycAtFetch)
              << std::endl;
}

bool RunNestest(const std::string& binPath, size_t maxInstructions)
{
    FlatBus bus;
    Cpu6502 cpu(&bus);

    const uint16_t programBase = 0xC000;
    if (!LoadBinaryToBus(bus, binPath, programBase)) {
        return false;
    }

    cpu.PC = programBase;
    cpu.SP = 0xFD;
    cpu.status = 0x24;

    size_t instructionBudget = maxInstructions;

    uint64_t totalCycles = 7;

    for (size_t i = 0; i < instructionBudget; ++i) {
        PrintCpuStateLine(cpu, bus, totalCycles);

        do {
            cpu.clock();
            ++totalCycles;
        } while (!cpu.instructionComplete());
    }

    //if (!DumpMemoryToLog(bus, "results.log")) {
    //    std::cerr << "Failed to write results.log" << std::endl;
    //    return false;
    //}
    return true;
}

int RunNestestMain()
{
    const std::string binPath = "6502_65C02_functional_tests\\bin_files\\nestest.prg.bin";
    const size_t maxInstructions = 5003;

    const bool ok = RunNestest(binPath, maxInstructions);
    return ok ? 0 : 1;
}