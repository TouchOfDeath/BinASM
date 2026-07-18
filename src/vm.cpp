// =============================================================================
// vm.cpp - The 8-bit Virtual Machine + integrated two-pass assembler.
// =============================================================================
//
//  This file has been extended to integrate the Pass-0 macro preprocessor
//  (see preprocessor.h). The high-level flow is now:
//
//     source text
//         |
//         v
//     [Preprocessor] -----> PreprocessedSource { text, line_map }
//         |
//         v
//     [Assembler Pass 1]   labels, constants, addresses
//         |
//         v
//     [Assembler Pass 2]   machine bytes -> memory[16]
//         |
//         v
//     Diagnostics (with ORIGINAL line numbers via line_map)
//
//  Every diagnostic emitted by the assembler carries an *expanded* line
//  number from Pass 1/2. Before returning diagnostics to the caller, we
//  translate each one through `line_map.originalLine(...)` so the editor's
//  red squiggles point at the user's source, not at an intermediate
//  expansion of a macro body.
//
// =============================================================================

#include "vm.h"
#include "isa.h"
#include "preprocessor.h"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <algorithm>
#include <cctype>

// -----------------------------------------------------------------------------
//  stringToByte - parses an 8-character binary string like "00011110" into
//  the corresponding byte. Kept for backwards compatibility with the
//  original code.
// -----------------------------------------------------------------------------
uint8_t VM::stringToByte(const std::string& str) {
    uint8_t val = 0;
    for (int i = 0; i < 8 && i < (int)str.length(); ++i) {
        if (str[i] == '1') {
            val |= (1 << (7 - i));
        }
    }
    return val;
}

VM::VM() {
    reset();
}

void VM::reset() {
    for (int i = 0; i < 16; ++i) memory[i] = 0;
    accumulator = 0;
    pc = 0;
    output_reg = 0;
    halted = false;
    carry_flag = false;
    zero_flag  = false;
    console_output.clear();
}

// =============================================================================
//  Anonymous-namespace helpers: tokenizer, integer parser, opcode table,
//  and the Assembler struct itself. These are unchanged from the original
//  implementation except where noted.
// =============================================================================
namespace {
    struct Token {
        std::string text;
        int line_num;   // 1-based, refers to the EXPANDED source.
    };

    // Tokenize a single line into whitespace-separated tokens, honoring
    // quoted strings and `#` comments.
    std::vector<Token> tokenizeLine(const std::string& line, int line_num) {
        std::vector<Token> tokens;
        std::string current = "";
        bool in_string = false;

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];
            if (c == '#') break; // Comment
            if (c == '"') {
                in_string = !in_string;
                current += c;
                if (!in_string) {
                    tokens.push_back({current, line_num});
                    current = "";
                }
                continue;
            }
            if (in_string) {
                current += c;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r') {
                if (!current.empty()) {
                    tokens.push_back({current, line_num});
                    current = "";
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) tokens.push_back({current, line_num});
        return tokens;
    }

    std::string toUpper(std::string s) {
        for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        return s;
    }

    bool isBinaryLiteral(const std::string& s) {
        if (s.length() < 2) return false;
        for (char c : s) if (c != '0' && c != '1') return false;
        return true;
    }

    bool parseInteger(std::string s, uint8_t& out_val, int& out_bits) {
        if (isBinaryLiteral(s)) {
            out_val = 0;
            for(char c : s) {
                out_val = (out_val << 1) | (c - '0');
            }
            out_bits = s.length();
            return true;
        }
        if (s.length() > 2 && s.substr(0, 2) == "0x") {
            try {
                out_val = std::stoi(s.substr(2), nullptr, 16);
                out_bits = (s.length() - 2) * 4; // roughly
                return true;
            } catch(...) { return false; }
        }
        if (!s.empty() && s.back() == 'd') {
            try {
                out_val = std::stoi(s.substr(0, s.length() - 1));
                out_bits = out_val > 15 ? 8 : 4;
                return true;
            } catch(...) { return false; }
        }
        // Single digit 0-9 implicitly decimal
        if (s.length() == 1 && isdigit(static_cast<unsigned char>(s[0]))) {
            out_val = s[0] - '0';
            out_bits = 4;
            return true;
        }
        return false;
    }

    // Built from isa.h ISA_TABLE — eliminates the last duplicate of the opcode
    // table.  isa.h is now the single authoritative source for all 11 opcodes.
    std::map<std::string, uint8_t> opcodes = [] {
        std::map<std::string, uint8_t> m;
        for (int i = 0; i < ISA_COUNT; ++i)
            m[ISA_TABLE[i].mnemonic] = ISA_TABLE[i].opcode;
        return m;
    }();

    // The Assembler now accepts a const reference to the preprocessed
    // source so it can translate expanded line numbers back to original
    // source lines when reporting diagnostics.
    struct Assembler {
        std::map<std::string, uint8_t> symbols;
        std::vector<VM::Diagnostic> diagnostics;
        uint8_t mem[16] = {0};
        bool mem_written[16] = {false};
        const PreprocessedSource* pp = nullptr;  // optional, may be nullptr

        // Translate an expanded line number to the original source line
        // using the preprocessor's line map. Falls through to the same
        // number if no map is available.
        int originalLine(int expanded_line) const {
            if (pp && expanded_line > 0) return pp->originalLine(expanded_line);
            return expanded_line;
        }

        void error(int expanded_line, const std::string& msg) {
            diagnostics.push_back({originalLine(expanded_line), true, "Error: " + msg});
        }
        void warn(int expanded_line, const std::string& msg) {
            diagnostics.push_back({originalLine(expanded_line), false, "Warning: " + msg});
        }

        // -------------------------------------------------------------------
        //  assemble() - runs Pass 1 (symbols) and Pass 2 (code gen) on the
        //  EXPANDED source text produced by the preprocessor.
        // -------------------------------------------------------------------
        void assemble(const std::string& text) {
            std::istringstream iss(text);
            std::string line;
            int line_num = 1;   // 1-based line number in the EXPANDED text.
            std::vector<std::vector<Token>> lines;

            while (std::getline(iss, line)) {
                lines.push_back(tokenizeLine(line, line_num++));
            }

            // ---------- Pass 1: Labels, Constants, Memory Directives ----------
            int pc = 0;
            for (auto& tline : lines) {
                if (tline.empty()) continue;

                std::string first = tline[0].text;
                if (first.length() > 0 && first.back() == ':') {
                    symbols[first.substr(0, first.length() - 1)] = pc;
                    tline.erase(tline.begin());
                    if (tline.empty()) continue;
                    first = tline[0].text;
                }

                if (tline.size() >= 3 && tline[1].text == ":=") {
                    uint8_t val; int bits;
                    if (parseInteger(tline[2].text, val, bits)) {
                        symbols[tline[0].text] = val;
                    } else {
                        error(tline[0].line_num, "Invalid constant value");
                    }
                    tline.clear();
                    continue;
                }

                if (first.length() > 1 && first[0] == '@') {
                    uint8_t addr; int bits;
                    if (parseInteger(first.substr(1), addr, bits)) {
                        pc = addr;
                        if (pc >= 16) {
                            error(tline[0].line_num, "Address out of bounds (0-15)");
                        }
                    } else {
                        error(tline[0].line_num, "Invalid address format");
                    }
                    tline.erase(tline.begin());
                    if (tline.empty()) continue;
                }

                if (toUpper(first) == "..." && tline.size() > 1) {
                    pc = 16;
                    continue;
                }

                if (pc < 16) pc++;
            }

            // ---------- Pass 2: Code Generation ----------
            pc = 0;
            bool found_hlt = false;
            bool acc_loaded = false;

            for (auto& tline : lines) {
                if (tline.empty()) continue;

                std::string first = tline[0].text;
                if (first.length() > 1 && first[0] == '@') {
                    uint8_t addr; int bits;
                    parseInteger(first.substr(1), addr, bits);
                    pc = addr;
                    tline.erase(tline.begin());
                    if (tline.empty()) continue;
                    first = tline[0].text;
                }

                if (toUpper(first) == "...") {
                    uint8_t val = 0; int bits;
                    if (parseInteger(tline[1].text, val, bits)) {
                        while (pc < 16) {
                            if (!mem_written[pc]) {
                                mem[pc] = val;
                                mem_written[pc] = true;
                            }
                            pc++;
                        }
                    }
                    continue;
                }

                if (pc >= 16) {
                    error(tline[0].line_num, "Code exceeds 16 bytes of memory");
                    continue;
                }

                if (mem_written[pc]) {
                    warn(tline[0].line_num, "Memory overlap: Writing to address " + std::to_string(pc) + " which was already populated.");
                }

                uint8_t final_byte = 0;
                int total_bits = 0;

                if (toUpper(first) == "DATA" && tline.size() > 1) {
                    std::string arg = tline[1].text;
                    if (arg.length() >= 3 && arg[0] == '"' && arg.back() == '"') {
                        final_byte = arg[1];
                        total_bits = 8;
                    } else {
                        uint8_t val; int bits;
                        if (parseInteger(arg, val, bits)) {
                            final_byte = val;
                            total_bits = 8;
                        } else {
                            error(tline[0].line_num, "Invalid DATA argument");
                        }
                    }
                } else {
                    for (const auto& token : tline) {
                        std::string up = toUpper(token.text);
                        if (opcodes.count(up)) {
                            final_byte = (final_byte << 4) | opcodes[up];
                            total_bits += 4;
                            if (up == "HLT") found_hlt = true;
                            if (up == "LDA" || up == "LDI") acc_loaded = true;
                            if (up == "OUT" && !acc_loaded) {
                                warn(token.line_num, "Outputting Accumulator before loading it.");
                            }
                        } else if (symbols.count(token.text)) {
                            final_byte = (final_byte << 4) | (symbols[token.text] & 0xF);
                            total_bits += 4;
                        } else {
                            uint8_t val; int bits;
                            if (parseInteger(token.text, val, bits)) {
                                if (bits > 4 && total_bits == 4) {
                                    error(token.line_num, "Operand Overflow: Instruction exceeds 8 bits.");
                                } else {
                                    if (bits <= 4) {
                                        final_byte = (final_byte << 4) | (val & 0xF);
                                        total_bits += 4;
                                    } else {
                                        final_byte = val;
                                        total_bits += 8;
                                    }
                                }
                            } else {
                                error(token.line_num, "Unknown identifier '" + token.text + "'");
                            }
                        }
                    }
                }

                if (total_bits != 8) {
                    error(tline[0].line_num, "Line must resolve to exactly 8 bits. Resolved to " + std::to_string(total_bits) + " bits.");
                } else {
                    mem[pc] = final_byte;
                    mem_written[pc] = true;
                    pc++;
                }
            }

            if (!found_hlt) {
                warn(-1, "Missing HLT instruction. CPU may execute raw data.");
            }
        }
    };
} // anonymous namespace

// -----------------------------------------------------------------------------
//  VM::loadProgram - preprocess, then assemble, then copy bytes into memory.
// -----------------------------------------------------------------------------
void VM::loadProgram(const std::string& binary_text) {
    reset();

    // Pass 0: macro preprocessor.
    Preprocessor pp;
    PreprocessedSource expanded = pp.process(binary_text);
    last_pp_ = std::make_shared<PreprocessedSource>(std::move(expanded));

    // Passes 1 & 2: assembler.
    Assembler asmblr;
    asmblr.pp = last_pp_.get();
    asmblr.assemble(last_pp_->text);

    for (int i = 0; i < 16; ++i) {
        memory[i] = asmblr.mem[i];
    }
}

// -----------------------------------------------------------------------------
//  VM::analyzeProgram - preprocess, then assemble, then return diagnostics
//  with ORIGINAL line numbers. Does NOT touch the VM's memory.
// -----------------------------------------------------------------------------
std::vector<VM::Diagnostic> VM::analyzeProgram(const std::string& binary_text) {
    Preprocessor pp;
    PreprocessedSource expanded = pp.process(binary_text);
    last_pp_ = std::make_shared<PreprocessedSource>(std::move(expanded));

    Assembler asmblr;
    asmblr.pp = last_pp_.get();
    asmblr.assemble(last_pp_->text);
    return asmblr.diagnostics;
}

// -----------------------------------------------------------------------------
//  VM::run - simple loop; unused by main.cpp but kept for completeness.
//
//  Bug fix (B2): the original `while (!halted) step()` had no step limit.
//  A program containing `JMP 0` would loop forever on the calling thread,
//  freezing the UI permanently. We now cap execution at 16*16 = 256 steps
//  (far more than any 16-byte program needs to run to completion), after
//  which we force-halt the VM and record a diagnostic message. The UI-driven
//  step / auto-step path is unaffected (it calls step() once per timer tick).
// -----------------------------------------------------------------------------
void VM::run() {
    constexpr int kMaxSteps = 256;
    int steps = 0;
    while (!halted && steps < kMaxSteps) {
        step();
        ++steps;
    }
    if (!halted) {
        halted = true;
        console_output.push_back(
            "RUNAWAY: VM halted after " + std::to_string(kMaxSteps) +
            " steps — possible infinite loop (JMP back to itself?). "
            "Use Step mode to inspect.");
    }
}

// -----------------------------------------------------------------------------
//  VM::step - execute one instruction. Unchanged from the original
//  implementation. (Note: LDI, JC, JZ opcodes are present in the opcode
//  table but not handled here; they will be reported as "Unknown opcode"
//  by the VM. The decompiler still understands them so it can decompile
//  binaries assembled by other tools.)
// -----------------------------------------------------------------------------
void VM::step() {
    if (halted) return;

    // Bug fix (B1): guard the memory read BEFORE incrementing PC.
    // If pc is >= 16 (e.g. forced by the hex editor or a corrupted jump),
    // halt gracefully instead of reading memory[16] — which is out-of-bounds
    // (memory[] is exactly 16 bytes) and is undefined behaviour.
    if (pc >= 16) {
        halted = true;
        console_output.push_back(
            "FAULT: PC=" + std::to_string(pc) +
            " is out of the 16-byte address space. Execution halted.");
        return;
    }

    uint8_t instr = memory[pc];
    uint8_t opcode = instr >> 4;
    uint8_t operand = instr & 0x0F;

    pc++;
    if (pc >= 16) halted = true; // Safety: will halt cleanly at the next step()

    switch (opcode) {
        case 0x0: // NOP
            console_output.push_back("NOP");
            break;
        case 0x1: // LDA
            accumulator = memory[operand];
            zero_flag = (accumulator == 0);
            console_output.push_back("LDA: Loaded " + std::to_string(accumulator));
            break;
        case 0x2: { // ADD
            uint16_t result = (uint16_t)accumulator + (uint16_t)memory[operand];
            carry_flag = (result > 255);
            accumulator = (uint8_t)(result & 0xFF);
            zero_flag = (accumulator == 0);
            console_output.push_back("ADD: Accumulator is now " + std::to_string(accumulator) +
                                     (carry_flag ? "  [CARRY]": ""));
            break;
        }
        case 0x3: { // SUB
            carry_flag = (memory[operand] > accumulator); // borrow
            accumulator -= memory[operand];
            zero_flag = (accumulator == 0);
            console_output.push_back("SUB: Accumulator is now " + std::to_string(accumulator) +
                                     (carry_flag ? "  [BORROW]" : ""));
            break;
        }
        case 0x4: // STA
            memory[operand] = accumulator;
            console_output.push_back("STA: Stored " + std::to_string(accumulator) + " at " + std::to_string(operand));
            break;
        case 0x5: // LDI - Load Immediate
            accumulator = operand;
            zero_flag = (accumulator == 0);
            console_output.push_back("LDI: Loaded immediate " + std::to_string(operand));
            break;
        case 0x6: // JMP
            pc = operand;
            console_output.push_back("JMP: Jumped to " + std::to_string(operand));
            break;
        case 0x7: // JC - Jump if Carry
            if (carry_flag) {
                pc = operand;
                console_output.push_back("JC: Carry set, jumped to " + std::to_string(operand));
            } else {
                console_output.push_back("JC: Carry clear, no jump");
            }
            break;
        case 0x8: // JZ - Jump if Zero
            if (zero_flag) {
                pc = operand;
                console_output.push_back("JZ: Zero set, jumped to " + std::to_string(operand));
            } else {
                console_output.push_back("JZ: Zero clear, no jump");
            }
            break;
        case 0xE: // OUT
            output_reg = accumulator;
            console_output.push_back("OUT: " + std::to_string(output_reg) + " (ASCII: '" + (char)(output_reg >= 32 ? output_reg : '.') + "')");
            break;
        case 0xF: // HLT
            halted = true;
            console_output.push_back("HLT: Execution halted.");
            break;
        default:
            console_output.push_back("Unknown opcode: " + std::to_string(opcode));
            break;
    }
}
