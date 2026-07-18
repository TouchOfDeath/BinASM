#pragma once
// =============================================================================
// panel_editor.h  —  Tabbed binary source editor panel with premium toolbar.
//
// Renders the "Binary Editor" ImGui window.  Toolbar buttons push AppEvents
// instead of directly calling VM methods, keeping UI decoupled from simulation.
// =============================================================================

struct AppState;

// Render the "Binary Editor" docked window, including:
//   • VS Code-style reorderable tab bar.
//   • Cosmic Aurora premium toolbar (Run / Step / Reset / Save / Load /
//     Decompile icon buttons, with animated hover and tooltips).
//   • ImGuiColorTextEdit code editor with live error markers already set
//     by AppState::update_lint().
//   • Save-error modal popup.
//
// Event side-effects pushed to state.events:
//   Run, Step, Reset, Save, Load, Decompile, NewTab, CloseTab.
void RenderBinaryEditor(AppState& state);
