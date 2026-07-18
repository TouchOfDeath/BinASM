#pragma once
// =============================================================================
// panel_debug_assist.h  —  Live lint diagnostics assistant panel.
// =============================================================================

struct AppState;

// Render the "Debug Assistant" docked window.
// Reads state.has_errors, state.error_markers, and state.warnings (all
// pre-computed by AppState::update_lint()) and presents them as annotated
// status indicators.
void RenderDebugAssistant(const AppState& state);
