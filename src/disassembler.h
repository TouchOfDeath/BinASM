#pragma once
// =============================================================================
// disassembler.h  —  Legacy linear disassembler.
//
// Parses raw .bincode text (8-character binary lines) and produces a
// human-readable mnemonic listing. This is a simpler linear scan — it does
// NOT handle macros or labels. The full assembler pipeline in vm.cpp is the
// authoritative path for assembling and executing programs.
//
// Kept for the "Live Disassembler" panel so the user can compare the linear
// view against the decompiler's reachability-based output.
// =============================================================================

#include <string>

// Parse `binary_text` line by line and return a formatted disassembly string.
// Each output line has the form:   Line N: MNEM [Addr X] / #val (immediate) / (no operand)
std::string disassembleBinary(const std::string& binary_text);
