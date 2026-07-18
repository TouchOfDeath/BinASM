#pragma once
// =============================================================================
// panel_decompiled.h  —  Reverse-engineered source output panel.
// =============================================================================

struct AppState;

// Render the "Decompiled Output" docked window.
// Reads state.decompiler.getSourceText() and displays it as a read-only
// code view.  Pushes AppEventKind::Decompile when the manual refresh
// button is clicked.
void RenderDecompiledOutput(AppState& state);
