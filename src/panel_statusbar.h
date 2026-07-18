#pragma once
// =============================================================================
// panel_statusbar.h  —  Premium status bar overlay.
// =============================================================================

class VM;

// Renders the thin status bar pinned to the bottom of the main viewport.
// Must be called inside the ImGui frame (after NewFrame, before Render).
//
// vm_running — true while the VM is executing instructions automatically.
// vm_halted  — true after HLT has been executed.
// vm         — read-only reference for register / flag values.
// has_errors — true if the current source has assembly errors.
void RenderStatusBar(bool vm_running, bool vm_halted, const VM& vm, bool has_errors);
