#pragma once
// =============================================================================
// panel_macro.h  —  Pass-0 macro inspector panel.
// =============================================================================

struct AppState;

// Render the "Macro Inspector" docked window.
// Reads state.macro_cache (pre-computed by AppState::update_macros()) and
// shows a table of defined macros plus a collapsible expanded-source view.
void RenderMacroInspector(const AppState& state);
