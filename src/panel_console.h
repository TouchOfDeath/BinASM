#pragma once
// =============================================================================
// panel_console.h  —  VM output stream / console panel.
// =============================================================================

struct AppState;

// Render the "Console Output" docked window.
// Displays vm.console_output as a scrolling log; auto-scrolls to bottom.
void RenderConsole(const AppState& state);
