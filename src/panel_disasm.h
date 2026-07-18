#pragma once
// =============================================================================
// panel_disasm.h  —  Live linear disassembler panel.
//
// Note: "disassembler.h" contains the disassembleBinary() function.
//       This header declares the ImGui panel that *displays* the output.
// =============================================================================

struct AppState;

// Render the "Live Disassembler" docked window.
// Reads state.disasm_cache.result (pre-computed by AppState::update_disasm())
// and renders each line with syntax-highlighted mnemonic coloring.
void RenderLiveDisassembler(const AppState& state);
