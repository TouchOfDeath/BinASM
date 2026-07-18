#pragma once
// =============================================================================
// panel_vm_state.h  —  8-bit CPU register / flag / memory-map panel.
// =============================================================================

struct AppState;

// Render the "VM State (8-bit CPU)" docked window.  Shows:
//   • Accumulator, PC, Output register — drawn as glass cards.
//   • Carry and Zero flag indicators with pulse animation.
//   • 16-byte memory map with binary representation; current PC highlighted.
void RenderVMState(AppState& state);
