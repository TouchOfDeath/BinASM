#pragma once
// =============================================================================
// fonts.h  —  Font loading for the Binary IDE.
//
// Loads two fonts into the ImGui font atlas:
//   Font 0 — UI sans-serif (proportional) at 15 px.
//   Font 1 — Editor monospace (JetBrains Mono / Fira Code) at 14.5 px.
//
// Both fonts attempt to merge FontAwesome icons for icon-button support.
// The loader silently falls back through candidates until one is found; the
// app always boots, worst-case with ImGui's built-in ProggyClean.
// =============================================================================

struct ImGuiIO;
struct ImFont;

// Global handle for the editor (mono) font.
// Pushed around the TextEditor widget so code stays properly aligned.
// Set by LoadFonts(); remains valid for the lifetime of the ImGui context.
extern ImFont* g_editor_font;

// Probe an array of file paths (null-terminated) and return the first one
// that exists and is readable, or nullptr if none are found.
const char* probe_font_path(const char** paths);

// Load the UI and editor fonts into `io.Fonts`.
// Call before io.Fonts->Build().
void LoadFonts(ImGuiIO& io);
