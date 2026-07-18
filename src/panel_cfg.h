#pragma once
// =============================================================================
// panel_cfg.h  —  Control Flow Graph visualiser panel.
// =============================================================================

class Decompiler;

// Renders the "Control Flow Graph" ImGui window.
//
// title      — ImGui window ID string (must match the DockBuilder assignment).
// decompiler — source of CFG data (call decompiler.decompile() before this).
// vm_running — when true, animated data-flow dots travel along edges.
void RenderCFGWindow(const char* title, const Decompiler& decompiler, bool vm_running);
