#pragma once
// =============================================================================
// isa.h  —  Authoritative ISA definition for the 8-bit educational VM.
//
// This header is the single source of truth for the 11 defined opcodes.
// All code that needs opcode names, binary encodings, or descriptions
// (disassembler, cheat sheet, assembler helpers) should include this
// header instead of duplicating the table.
// =============================================================================

#include <cstdint>
#include <cstring>   // strcmp
#include <string>

// ---------------------------------------------------------------------------
//  Opcode table — 11 instructions in ISA order.
// ---------------------------------------------------------------------------
struct IsaEntry {
    const char* mnemonic;    // Uppercase mnemonic string (e.g., "LDA")
    uint8_t     opcode;      // 4-bit opcode value (0x0 – 0xF)
    const char* bits;        // Human-readable 4-bit binary string (e.g., "0001")
    bool        has_operand; // true  → operand is a 4-bit RAM address
                             // false → operand field unused (NOP/OUT/HLT)
    bool        is_immediate;// true  → operand is an immediate value (LDI only)
    const char* description; // One-line description for the cheat sheet
};

// The table is constexpr so it can be used in constant expressions and
// costs zero runtime initialisation.
inline constexpr IsaEntry ISA_TABLE[] = {
    { "NOP", 0x0, "0000", false, false, "No Operation (do nothing)"               },
    { "LDA", 0x1, "0001", true,  false, "Load Accumulator from Memory Address"    },
    { "ADD", 0x2, "0010", true,  false, "Add Memory Address to Accumulator"       },
    { "SUB", 0x3, "0011", true,  false, "Subtract Memory Address from Accumulator"},
    { "STA", 0x4, "0100", true,  false, "Store Accumulator to Memory Address"     },
    { "LDI", 0x5, "0101", true,  true,  "Load Immediate value to Accumulator"     },
    { "JMP", 0x6, "0110", true,  false, "Unconditional Jump"                      },
    { "JC",  0x7, "0111", true,  false, "Jump if Carry flag is set"               },
    { "JZ",  0x8, "1000", true,  false, "Jump if Zero flag is set"                },
    { "OUT", 0xE, "1110", false, false, "Output Accumulator to Console"           },
    { "HLT", 0xF, "1111", false, false, "Halt Execution"                          },
};
inline constexpr int ISA_COUNT = static_cast<int>(sizeof(ISA_TABLE) / sizeof(ISA_TABLE[0]));

// ---------------------------------------------------------------------------
//  Convenience helpers.
// ---------------------------------------------------------------------------

// Returns the mnemonic for a 4-bit opcode value, or "???" if unknown.
inline const char* isa_mnemonic(uint8_t opcode) {
    for (int i = 0; i < ISA_COUNT; ++i) {
        if (ISA_TABLE[i].opcode == opcode) return ISA_TABLE[i].mnemonic;
    }
    return "???";
}

// Returns the IsaEntry* for a mnemonic string (case-insensitive), or nullptr.
inline const IsaEntry* isa_find_mnemonic(const char* mnem) {
    for (int i = 0; i < ISA_COUNT; ++i) {
        // Simple ASCII case-insensitive compare.
        const char* a = ISA_TABLE[i].mnemonic;
        const char* b = mnem;
        bool match = true;
        while (*a && *b) {
            char ca = (*a >= 'a' && *a <= 'z') ? (*a - 32) : *a;
            char cb = (*b >= 'a' && *b <= 'z') ? (*b - 32) : *b;
            if (ca != cb) { match = false; break; }
            ++a; ++b;
        }
        if (match && *a == '\0' && *b == '\0') return &ISA_TABLE[i];
    }
    return nullptr;
}
