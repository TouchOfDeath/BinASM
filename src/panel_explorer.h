#pragma once
// =============================================================================
// panel_explorer.h  —  Workspace file explorer sidebar.
// =============================================================================

struct AppState;

// Render the "Explorer" docked sidebar window.  Shows:
//   • Open editor tabs (click to switch, right-click to close).
//   • Workspace .bincode files (click to open in a new tab).
//   • Refresh + New File buttons.
//
// Pushes NewTab, CloseTab, OpenFile events to state.events.
void RenderExplorer(AppState& state);
