// =============================================================================
// decompiler.cpp - Implementation of the Decompiler & CFG Generator.
// =============================================================================
//
//  See decompiler.h for the architectural overview. This file implements:
//
//    Stage 1  decodeAll         - byte -> (opcode, operand, mnemonic)
//    Stage 2  identifyJumpTargets - scan for JMP/JC/JZ and tag targets
//    Stage 3  classifyCodeData  - BFS reachability from entry point
//    Stage 4  buildBasicBlocks  - split into maximal straight-line runs
//    Stage 5  buildCFGEdges     - wire blocks together based on terminator
//    Stage 6  emitSource        - render the final .bincode text
//
//  ISA reference (must stay in sync with vm.cpp):
//    0x0 NOP    0x1 LDA    0x2 ADD    0x3 SUB
//    0x4 STA    0x5 LDI    0x6 JMP    0x7 JC
//    0x8 JZ     0xE OUT    0xF HLT
//
//  The 4-bit operand is interpreted as a memory address for LDA/ADD/SUB/STA/
//  JMP/JC/JZ, as an immediate for LDI, and is unused for NOP/OUT/HLT.
//
// =============================================================================

#include "decompiler.h"

#include <sstream>
#include <iomanip>
#include <set>
#include <queue>
#include <algorithm>

// -----------------------------------------------------------------------------
//  Opcode table - mirrors vm.cpp's `opcodes` map. Kept local to this TU so
//  the decompiler remains a self-contained module.
// -----------------------------------------------------------------------------
namespace {
    struct OpcodeInfo {
        const char* mnemonic;
        bool        has_addr_operand;   // operand is a memory address (0..15)
        bool        is_branch;          // unconditional or conditional jump
        bool        is_terminator;      // HLT (no fall-through)
    };

    // Indexed by the 4-bit opcode (0..15). Unknown opcodes are treated as
    // `DATA` so they survive a round-trip through the assembler.
    constexpr OpcodeInfo kOpcodeTable[16] = {
        /* 0x0 */ {"NOP", false, false, false},
        /* 0x1 */ {"LDA", true,  false, false},
        /* 0x2 */ {"ADD", true,  false, false},
        /* 0x3 */ {"SUB", true,  false, false},
        /* 0x4 */ {"STA", true,  false, false},
        /* 0x5 */ {"LDI", false, false, false},  // immediate operand
        /* 0x6 */ {"JMP", true,  true,  false},  // unconditional jump
        /* 0x7 */ {"JC",  true,  true,  false},  // jump if carry
        /* 0x8 */ {"JZ",  true,  true,  false},  // jump if zero
        /* 0x9 */ {"DATA",false, false, false},  // reserved/unknown
        /* 0xA */ {"DATA",false, false, false},
        /* 0xB */ {"DATA",false, false, false},
        /* 0xC */ {"DATA",false, false, false},
        /* 0xD */ {"DATA",false, false, false},
        /* 0xE */ {"OUT", false, false, false},
        /* 0xF */ {"HLT", false, false, true }
    };
}

// -----------------------------------------------------------------------------
//  Constructor / reset.
// -----------------------------------------------------------------------------
Decompiler::Decompiler() {
    instructions_.reserve(16);
}

// -----------------------------------------------------------------------------
//  Top-level pipeline. Reset all state then run all six stages in order.
// -----------------------------------------------------------------------------
void Decompiler::decompile(const uint8_t memory[16]) {
    instructions_.clear();
    labels_.clear();
    cfg_.blocks.clear();
    cfg_.edges.clear();
    source_text_.clear();

    decodeAll(memory);
    identifyJumpTargets();
    classifyCodeData();
    buildBasicBlocks();
    buildCFGEdges();
    emitSource();
}

// -----------------------------------------------------------------------------
//  Stage 1: Decode every byte into an instruction record. We do not yet
//  decide code-vs-data; every byte gets a tentative mnemonic based on its
//  upper nibble. Unknown opcodes (0x9..0xD) become "DATA".
// -----------------------------------------------------------------------------
void Decompiler::decodeAll(const uint8_t memory[16]) {
    for (int addr = 0; addr < 16; ++addr) {
        uint8_t byte = memory[addr];
        DecodedInstruction di;
        di.address        = static_cast<uint8_t>(addr);
        di.opcode         = (byte >> 4) & 0x0F;
        di.operand        = byte & 0x0F;
        di.mnemonic       = kOpcodeTable[di.opcode].mnemonic;
        di.is_data        = false;
        di.is_jump_target = false;
        di.label.clear();
        instructions_.push_back(di);
    }
}

// -----------------------------------------------------------------------------
//  Stage 2: Scan the linear instruction stream for branch instructions
//  (JMP / JC / JZ) and mark their operands as jump targets. Each target
//  address gets a synthetic label of the form "loc_<addr>".
//
//  Important: even if the target address points to a byte we will later
//  classify as data, we still record the label. This preserves intent
//  and the user can spot "jump into data" bugs in the decompiled output.
// -----------------------------------------------------------------------------
void Decompiler::identifyJumpTargets() {
    for (const auto& di : instructions_) {
        if (!kOpcodeTable[di.opcode].is_branch) continue;
        // The operand is the target address (0..15).
        uint8_t target = di.operand;
        if (labels_.find(target) == labels_.end()) {
            std::string name = "loc_" + std::to_string(target);
            labels_[target] = name;
        }
    }
    // Annotate the instructions themselves.
    for (auto& di : instructions_) {
        auto it = labels_.find(di.address);
        if (it != labels_.end()) {
            di.is_jump_target = true;
            di.label = it->second;
        }
    }
}

// -----------------------------------------------------------------------------
//  Stage 3: Reachability analysis. Starting from address 0 (the VM entry
//  point), perform a BFS over the static control-flow graph. Any address
//  reached is marked as code; any address NOT reached is marked as data.
//
//  Reachability rules:
//    * From a normal instruction (NOP/LDA/ADD/.../OUT/LDI), control falls
//      through to address+1.
//    * From a JMP, control transfers ONLY to the operand.
//    * From a JC/JZ, control transfers to BOTH the operand AND address+1
//      (the fall-through case when the condition is false).
//    * From a HLT, control goes nowhere.
//
//  Note: We do NOT chase edges from addresses classified as data, because
//  a "JMP" instruction sitting inside a data block is just a coincidental
//  byte pattern, not real code.
// -----------------------------------------------------------------------------
void Decompiler::classifyCodeData() {
    if (instructions_.empty()) return;

    std::vector<bool> reached(16, false);
    std::queue<uint8_t> worklist;
    worklist.push(0);          // entry point
    reached[0] = true;

    while (!worklist.empty()) {
        uint8_t addr = worklist.front();
        worklist.pop();

        const auto& di = instructions_[addr];
        const auto& info = kOpcodeTable[di.opcode];

        // If this byte is "data" (i.e. its opcode nibble is in the
        // reserved/unknown range), treat it as a data byte and do not
        // follow any control flow from it.
        if (std::string(info.mnemonic) == "DATA") {
            // Fall through to next address; data bytes do not branch.
            if (addr + 1 < 16 && !reached[addr + 1]) {
                reached[addr + 1] = true;
                worklist.push(static_cast<uint8_t>(addr + 1));
            }
            continue;
        }

        if (info.is_terminator) {
            // HLT: no successors.
            continue;
        }
        if (info.is_branch) {
            // Conditional jumps have two successors; unconditional one.
            uint8_t target = di.operand;
            if (!reached[target]) {
                reached[target] = true;
                worklist.push(target);
            }
            // JC / JZ fall through if condition false. JMP does not.
            // We can distinguish by opcode: 0x6 is unconditional JMP.
            if (di.opcode != 0x6) {
                uint8_t next = static_cast<uint8_t>(addr + 1);
                if (next < 16 && !reached[next]) {
                    reached[next] = true;
                    worklist.push(next);
                }
            }
        } else {
            // Plain instruction: fall through.
            uint8_t next = static_cast<uint8_t>(addr + 1);
            if (next < 16 && !reached[next]) {
                reached[next] = true;
                worklist.push(next);
            }
        }
    }

    // Apply the classification. Anything not reached is data.
    // However, jump *targets* that are not reached should still be treated
    // as code (the user's intent was clearly to jump there). This catches
    // the rare case where the only path to a byte is via a JMP from a
    // block that itself is unreachable.
    for (auto& di : instructions_) {
        if (reached[di.address] || di.is_jump_target) {
            di.is_data = false;
        } else {
            di.is_data = true;
            di.mnemonic = "DATA";
        }
    }
}

// -----------------------------------------------------------------------------
//  Stage 4: Partition the instruction stream into basic blocks.
//
//  Block boundaries:
//    * A new block starts at address 0 (entry).
//    * A new block starts at any address that is a jump target.
//    * A new block starts at the instruction immediately following a
//      branch or terminator (because control may or may not arrive there).
//
//  A block ends as soon as one of the above conditions creates the next
//  block, OR at the end of memory.
// -----------------------------------------------------------------------------
void Decompiler::buildBasicBlocks() {
    cfg_.blocks.clear();

    // First pass: collect block start addresses.
    std::set<uint8_t> block_starts;
    block_starts.insert(0);
    for (const auto& di : instructions_) {
        if (di.is_data) continue;
        if (di.is_jump_target && di.address != 0) {
            block_starts.insert(di.address);
        }
        const auto& info = kOpcodeTable[di.opcode];
        if ((info.is_branch || info.is_terminator) && di.address + 1 < 16) {
            block_starts.insert(static_cast<uint8_t>(di.address + 1));
        }
    }

    // Second pass: emit blocks.
    // We iterate over `block_starts` in ascending order; each block runs
    // from its start address up to (but not including) the next start.
    std::vector<uint8_t> starts_sorted(block_starts.begin(),
                                       block_starts.end());
    for (size_t i = 0; i < starts_sorted.size(); ++i) {
        uint8_t start = starts_sorted[i];
        uint8_t end   = (i + 1 < starts_sorted.size())
                        ? static_cast<uint8_t>(starts_sorted[i+1] - 1)
                        : 15;

        BasicBlock bb;
        bb.id         = static_cast<int>(cfg_.blocks.size());
        bb.start_addr = start;
        bb.end_addr   = end;
        bb.terminates = false;
        bb.label      = instructions_[start].label;

        for (uint8_t a = start; a <= end; ++a) {
            // Stop early if we hit a non-code byte before `end`.
            if (instructions_[a].is_data) {
                // Bug fix (B3): uint8_t underflow guard.
                // If a==start (the very first byte of the block is data),
                // a-1 would underflow to 255 for uint8_t. In that case the
                // block has no valid instruction bytes at all; setting end_addr
                // to (start-1) makes the later empty() check discard the block.
                if (a == start) {
                    bb.end_addr = start; // will produce empty instructions => discarded
                } else {
                    bb.end_addr = static_cast<uint8_t>(a - 1);
                }
                break;
            }
            bb.instructions.push_back(instructions_[a]);
            const auto& info = kOpcodeTable[instructions_[a].opcode];
            if (info.is_terminator) {
                bb.terminates = true;
                bb.end_addr   = a;
                break;
            }
            if (info.is_branch) {
                bb.end_addr = a;
                break;
            }
        }

        if (!bb.instructions.empty()) {
            cfg_.blocks.push_back(std::move(bb));
        }
    }
}

// -----------------------------------------------------------------------------
//  Stage 5: Connect basic blocks with edges based on the terminator of
//  each block.
//
//  Edge rules:
//    * HLT-terminated block   -> no successors.
//    * JMP-terminated block   -> one successor: the block containing the
//                                 jump target address.
//    * JC/JZ-terminated block -> two successors: the block containing the
//                                 jump target, AND the block starting at
//                                 (end_addr + 1) if any.
//    * Block that ends because the next address is a block start (i.e.
//      falls through) -> one successor: that next block.
//    * Block that ends at end of memory with no terminator -> no successor.
// -----------------------------------------------------------------------------
void Decompiler::buildCFGEdges() {
    cfg_.edges.clear();

    // Helper: find the block index whose [start,end] range contains `addr`.
    auto findBlockContaining = [&](uint8_t addr) -> int {
        for (const auto& bb : cfg_.blocks) {
            if (addr >= bb.start_addr && addr <= bb.end_addr) return bb.id;
            // Also handle the case where addr is the start of the next block
            // (i.e. addr == end+1 and that is a block boundary).
            if (addr == bb.start_addr) return bb.id;
        }
        // Fallback: pick the block whose start is the smallest address
        // >= addr. This handles jumps that land exactly on a block start
        // that we may have missed in the linear scan.
        for (const auto& bb : cfg_.blocks) {
            if (bb.start_addr == addr) return bb.id;
        }
        return -1;
    };

    for (auto& bb : cfg_.blocks) {
        if (bb.instructions.empty()) continue;
        const DecodedInstruction& last = bb.instructions.back();
        const auto& info = kOpcodeTable[last.opcode];

        if (info.is_terminator) {
            // HLT - no successors.
            continue;
        }
        if (info.is_branch) {
            // Edge 1: jump target.
            int target_block = findBlockContaining(last.operand);
            if (target_block >= 0) {
                bb.successors.push_back(target_block);
                cfg_.edges.push_back({bb.id, target_block});
            }
            // Edge 2: fall-through (only for conditional jumps).
            if (last.opcode != 0x6) {  // not unconditional JMP
                uint8_t fall_addr = static_cast<uint8_t>(last.address + 1);
                if (fall_addr < 16) {
                    int fall_block = findBlockContaining(fall_addr);
                    if (fall_block >= 0 && fall_block != bb.id) {
                        bb.successors.push_back(fall_block);
                        cfg_.edges.push_back({bb.id, fall_block});
                    }
                }
            }
        } else {
            // Plain fall-through block.
            uint8_t fall_addr = static_cast<uint8_t>(bb.end_addr + 1);
            if (fall_addr < 16) {
                int fall_block = findBlockContaining(fall_addr);
                if (fall_block >= 0) {
                    bb.successors.push_back(fall_block);
                    cfg_.edges.push_back({bb.id, fall_block});
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Stage 6: Emit the final .bincode source text.
//
//  Output format (one line per address):
//     loc_5:           # synthetic label (only on jump targets)
//     LDA 3            # decoded instruction
//     ...
//     DATA 0x42        # inferred data byte
//
//  Comments carry the original address for easy cross-reference with the
//  hex editor. Labels are emitted on their own line so the existing
//  assembler can parse them with no changes.
// -----------------------------------------------------------------------------
void Decompiler::emitSource() {
    std::ostringstream oss;
    oss << "# =====================================\n";
    oss << "# Decompiled from VM memory snapshot\n";
    oss << "# " << cfg_.blocks.size() << " basic block(s), "
        << cfg_.edges.size() << " CFG edge(s)\n";
    oss << "# =====================================\n\n";

    for (const auto& di : instructions_) {
        // Synthetic label (if any) comes on its own line, just like in
        // hand-written .bincode.
        if (!di.label.empty()) {
            oss << di.label << ":\n";
        }

        // Render the instruction. The address is included as a trailing
        // comment (`# addr N`) so the output remains re-assemblable by the
        // existing assembler (which uses `@N` as a *standalone* directive,
        // not as an inline prefix).
        oss << "    ";

        if (di.is_data) {
            // Render data bytes in a way the existing assembler accepts:
            //   DATA <value>
            // The value is emitted in 0xNN hex form for compactness.
            oss << "DATA 0x"
                << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(((di.opcode << 4) | di.operand) & 0xFF)
                << std::dec;
        } else {
            const auto& info = kOpcodeTable[di.opcode];
            oss << di.mnemonic;
            if (info.has_addr_operand) {
                // If the operand points at a labeled address, prefer the label.
                auto it = labels_.find(di.operand);
                if (it != labels_.end()) {
                    oss << " " << it->second;
                } else {
                    oss << " " << static_cast<int>(di.operand) << "d";
                }
            } else if (di.opcode == 0x5 /* LDI */) {
                // LDI takes an immediate value, not an address.
                oss << " " << static_cast<int>(di.operand) << "d";
            }
            // NOP / OUT / HLT have no operand.
        }

        // Trailing address comment for cross-reference with the hex editor.
        // The leading `#` makes the assembler's lexer treat it as a comment.
        oss << "    # addr " << static_cast<int>(di.address);

        oss << "\n";
    }

    source_text_ = oss.str();
}

// -----------------------------------------------------------------------------
//  Public lookup helper: returns "loc_N" for an address, or "" if not a
//  jump target.
// -----------------------------------------------------------------------------
std::string Decompiler::labelFor(uint8_t address) const {
    auto it = labels_.find(address);
    return (it == labels_.end()) ? std::string() : it->second;
}

// -----------------------------------------------------------------------------
//  Static helpers (also exposed publicly via the header for callers that
//  only need a quick opcode classification without running the full
//  decompiler pipeline).
// -----------------------------------------------------------------------------
std::string Decompiler::mnemonicFor(uint8_t opcode) {
    return kOpcodeTable[opcode & 0x0F].mnemonic;
}
bool Decompiler::isBranch(uint8_t opcode) {
    return kOpcodeTable[opcode & 0x0F].is_branch;
}
bool Decompiler::isTerminator(uint8_t opcode) {
    return kOpcodeTable[opcode & 0x0F].is_terminator;
}
bool Decompiler::isControlFlow(uint8_t opcode) {
    const auto& info = kOpcodeTable[opcode & 0x0F];
    return info.is_branch || info.is_terminator;
}
