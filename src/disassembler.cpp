// =============================================================================
// disassembler.cpp  —  Legacy linear disassembler implementation.
//
// Uses isa.h as the single opcode table — no local duplication.
// =============================================================================

#include "disassembler.h"
#include "isa.h"

#include <sstream>
#include <string>

std::string disassembleBinary(const std::string& binary_text) {
    std::stringstream result;
    std::istringstream iss(binary_text);
    std::string line;
    int line_num = 1;

    while (std::getline(iss, line)) {
        // Strip comments.
        const size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line = line.substr(0, hash_pos);
        }

        // Collect just '0' and '1' characters.
        std::string bits;
        for (char c : line) {
            if (c == '0' || c == '1') bits += c;
        }

        if (bits.size() >= 8) {
            const std::string opcode_bits   = bits.substr(0, 4);
            const std::string operand_bits  = bits.substr(4, 4);

            // Decode 4-bit opcode to uint8_t.
            uint8_t opcode = 0;
            for (int i = 0; i < 4; ++i) {
                if (opcode_bits[i] == '1') opcode |= static_cast<uint8_t>(1 << (3 - i));
            }

            // Decode 4-bit operand.
            uint8_t operand = 0;
            for (int i = 0; i < 4; ++i) {
                if (operand_bits[i] == '1') operand |= static_cast<uint8_t>(1 << (3 - i));
            }

            // Look up mnemonic via isa.h — single source of truth.
            const char* mnem = isa_mnemonic(opcode);

            // Find the ISA entry for formatting context.
            const IsaEntry* entry = nullptr;
            for (int i = 0; i < ISA_COUNT; ++i) {
                if (ISA_TABLE[i].opcode == opcode) { entry = &ISA_TABLE[i]; break; }
            }

            result << "Line " << line_num << ": " << mnem;
            if (entry && entry->is_immediate) {
                result << " #" << static_cast<int>(operand) << " (immediate)";
            } else if (entry && entry->has_operand) {
                result << " [Addr " << static_cast<int>(operand) << "]";
            }
            result << '\n';

        } else if (!bits.empty()) {
            result << "Line " << line_num << ": Incomplete instruction (" << bits << ")\n";
        }

        ++line_num;
    }
    return result.str();
}
