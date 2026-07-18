// =============================================================================
// decompiler.h - Reverse-engineers raw VM memory bytes back into readable
//                .bincode source and builds a Control Flow Graph (CFG).
// =============================================================================
//
//  Feature 1 of the IDE upgrade:
//    "A Full Decompiler & Control Flow Graph (CFG) Generator"
//
//  Responsibilities:
//    1. Parse the raw 16-byte `VM::memory[]` array.
//    2. Reverse-engineer it back into highly readable .bincode syntax.
//    3. Automatically infer which addresses hold *code* vs *data*.
//    4. Reconstruct JMP / JC / JZ targets and inject synthetic labels
//       of the form `loc_<addr>:` so the output re-assembles cleanly.
//    5. (Stretch goal) Build a Control Flow Graph of basic blocks and the
//       edges between them, suitable for rendering with ImGui draw lists.
//
//  Design notes:
//    * The VM has a tiny 4-bit opcode + 4-bit operand ISA. Addresses 0..15
//      are reachable, so the decompiler can afford an O(N) work per byte
//      without any performance concerns.
//    * "Code vs Data" inference is performed with a mark-and-sweep style
//      reachability pass starting from address 0 (the entry point) and
//      following every static control-flow edge (fall-through, JMP, JC, JZ).
//      Bytes that are *referenced* as jump targets but never *reached* by
//      the linear sweep are still classified as code (the jump is the only
//      way to reach them). Bytes never referenced and never reached are
//      classified as data.
//    * The CFG is composed of "basic blocks": maximal runs of instructions
//      with no internal branches. A block ends at the first control-flow
//      instruction (JMP/JC/JZ/HLT) or at the instruction just before a
//      jump target.
//
// =============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

// -----------------------------------------------------------------------------
//  Decoded instruction (one byte of memory interpreted as code).
// -----------------------------------------------------------------------------
struct DecodedInstruction {
    uint8_t  address;      // Memory address (0..15) this byte occupies.
    uint8_t  opcode;       // Upper 4 bits of the byte.
    uint8_t  operand;      // Lower 4 bits of the byte.
    std::string mnemonic;  // "LDA", "ADD", "JMP", "HLT", or "DATA".
    bool     is_data;      // True if inferred to be a data byte, not code.
    bool     is_jump_target;// True if some branch instruction lands here.
    std::string label;     // "loc_<addr>" if is_jump_target, else "".
};

// -----------------------------------------------------------------------------
//  Basic Block: a maximal straight-line sequence of instructions with no
//  branches in the middle. Successors are indices into CFG::blocks.
// -----------------------------------------------------------------------------
struct BasicBlock {
    int      id;                       // 0-based index in CFG::blocks.
    uint8_t  start_addr;               // First memory address of the block.
    uint8_t  end_addr;                 // Last memory address (inclusive).
    std::vector<DecodedInstruction> instructions;
    std::vector<int> successors;       // Indices of successor blocks.
    std::string label;                 // Same as instructions_[0].label.
    bool     terminates;               // True if block ends in HLT (no fall-through).
};

// -----------------------------------------------------------------------------
//  Control Flow Graph: collection of basic blocks + edges.
//  Edges are stored both inside `blocks[i].successors` and flattened into
//  `edges` for convenient iteration when drawing.
// -----------------------------------------------------------------------------
struct CFG {
    std::vector<BasicBlock>           blocks;
    std::vector<std::pair<int,int>>   edges;  // (from_block_id, to_block_id)
};

// -----------------------------------------------------------------------------
//  The Decompiler itself.
// -----------------------------------------------------------------------------
class Decompiler {
public:
    Decompiler();

    // Run the full decompilation pipeline on a snapshot of VM memory.
    // `memory` is a pointer to at least 16 bytes.
    void decompile(const uint8_t memory[16]);

    // Returns the reconstructed .bincode source text.
    // Call only after decompile().
    const std::string& getSourceText() const { return source_text_; }

    // Returns the decoded instruction list (one entry per address).
    const std::vector<DecodedInstruction>& getInstructions() const {
        return instructions_;
    }

    // Returns the Control Flow Graph.
    const CFG& getCFG() const { return cfg_; }

    // Convenience: returns the label name for a given address, or "" if
    // the address is not a jump target.
    std::string labelFor(uint8_t address) const;

private:
    // ---------- Pipeline stages ----------
    // Stage 1: Decode every byte into (opcode, operand, mnemonic).
    void decodeAll(const uint8_t memory[16]);

    // Stage 2: Find all branch targets and mark them with labels.
    void identifyJumpTargets();

    // Stage 3: Reachability analysis - decide code vs data.
    void classifyCodeData();

    // Stage 4: Group instructions into basic blocks.
    void buildBasicBlocks();

    // Stage 5: Connect basic blocks with edges.
    void buildCFGEdges();

    // Stage 6: Emit human-readable .bincode text.
    void emitSource();

    // ---------- Helpers ----------
    static std::string mnemonicFor(uint8_t opcode);
    static bool        isBranch(uint8_t opcode);   // JMP / JC / JZ
    static bool        isTerminator(uint8_t opcode);// HLT
    static bool        isControlFlow(uint8_t opcode);// branch || terminator

    // ---------- State ----------
    std::vector<DecodedInstruction> instructions_;
    std::map<uint8_t, std::string>  labels_;       // addr -> "loc_N"
    CFG                             cfg_;
    std::string                     source_text_;
};
