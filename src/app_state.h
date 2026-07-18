// =============================================================================
// app_state.h  —  Centralized application state for the Binary IDE.
//
// All mutable program state previously scattered as locals in main() lives
// here.  Panel functions receive an AppState& and push typed AppEvents
// instead of calling VM methods directly.  The main loop calls
// state.process_events() + state.tick(now) once per frame to advance
// simulation and apply deferred side-effects.
//
// Architecture:
//   main()          — GLFW/ImGui lifecycle, DockBuilder, event loop shell
//   AppState        — owns VM, tabs, caches, explorer, modal flags
//   AppEvent        — panel → AppState message passing (typed union)
//   RenderXxx(AppState&) — each panel reads/pushes through AppState
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>

#include "TextEditor.h"
#include "vm.h"
#include "decompiler.h"
#include "hex_editor.h"
#include "editor_tab.h"
#include "preprocessor.h"

// =============================================================================
//  Event system — panels push events; process_events() consumes them.
//  Kept intentionally simple: a flat vector of tagged structs.  No observer
//  pattern needed in a single-threaded immediate-mode app.
// =============================================================================
enum class AppEventKind {
    Run,        // Assemble + reset + start VM.
    Step,       // Execute one instruction.
    Reset,      // Clear VM state.
    Save,       // Write active tab to disk (payload = optional override path).
    Load,       // Read workspace.bincode into active tab.
    Decompile,  // Force-refresh decompiler on next frame.
    NewTab,     // Open a blank editor tab.
    CloseTab,   // Close a specific tab (tab_index = which, -1 = active).
    OpenFile,   // Open a file path into a new tab (payload = filepath).
};

struct AppEvent {
    AppEventKind kind;
    std::string  payload;    // filepath for Save/Load/OpenFile, otherwise empty.
    int          tab_index;  // for CloseTab; -1 means active_tab.

    AppEvent(AppEventKind k, std::string p = {}, int ti = -1)
        : kind(k), payload(std::move(p)), tab_index(ti) {}
};

// =============================================================================
//  Per-frame caches — invalidated when editor source text changes.
//  Running analyzeProgram() and disassembleBinary() at 60 fps when nothing
//  has changed wastes CPU and triggers noisy redraws.
// =============================================================================
struct LintCache {
    std::string                 text;
    std::vector<VM::Diagnostic> diags;
    bool                        valid = false;
};

struct DisasmCache {
    std::string text;
    std::string result;
    bool        valid = false;
};

struct MacroCache {
    std::string          text;
    std::vector<Macro>   macros;
    std::string          expanded;  // Post-pass-0 source, for display.
    bool                 valid = false;
};

// =============================================================================
//  AppState — the single root of all application state.
// =============================================================================
struct AppState {
    // ---- Multi-tab editor ---------------------------------------------------
    std::vector<EditorTab> tabs;
    int                    active_tab = 0;

    // ---- VM + simulation timing ---------------------------------------------
    VM      vm;
    bool    vm_running     = false;
    bool    first_load     = true;    // True until Run/Step has been used once.
    double  last_step_time = 0.0;     // glfwGetTime() of the last auto-step.

    // ---- Analysis -----------------------------------------------------------
    Decompiler decompiler;
    HexEditor  hex_editor;
    bool       decompiler_dirty = true;

    // ---- Source analysis caches ---------------------------------------------
    LintCache   lint_cache;
    DisasmCache disasm_cache;
    MacroCache  macro_cache;

    // ---- File explorer ------------------------------------------------------
    std::vector<std::string> explorer_files;
    bool                     explorer_needs_refresh = true;

    // ---- Save-error modal ---------------------------------------------------
    bool        save_error_open = false;
    std::string save_error_path;
    std::string save_error_msg;

    // ---- Open-file modal (Ctrl+O) -------------------------------------------
    bool open_file_dialog_open = false;
    char open_file_input[256]  = {};

    // ---- Pending events (panels → simulation) -------------------------------
    std::vector<AppEvent> events;

    // ---- Derived state (updated each frame by update_lint) ------------------
    // Kept in AppState so multiple panels (editor + debug assistant) can read
    // the same pre-computed set without running the linter twice.
    TextEditor::ErrorMarkers    error_markers;
    std::vector<VM::Diagnostic> warnings;
    bool                        has_errors = false;

    // =========================================================================
    //  Lifecycle
    // =========================================================================

    // Populate the initial default tab.  Call once before the render loop.
    void init();

    // Advance the VM at ~2 Hz when vm_running && !vm.halted.
    // `now` is glfwGetTime() — the same clock used to set last_step_time.
    void tick(double now);

    // =========================================================================
    //  Event queue
    // =========================================================================

    void push_event(AppEventKind kind, std::string payload = {}, int tab_index = -1);

    // Consume all pending events.  Must be called once per frame, after
    // panel rendering (so panels have had a chance to push their events).
    void process_events();

    // =========================================================================
    //  Cache updates — called automatically inside update_frame().
    // =========================================================================

    // Run the linter and refresh error_markers / warnings / has_errors.
    void update_lint(const std::string& text);

    // Run the linear disassembler.
    void update_disasm(const std::string& text);

    // Run the preprocessor for the Macro Inspector.
    void update_macros(const std::string& text);

    // Scan the working directory for .bincode files.
    void refresh_explorer();

    // =========================================================================
    //  Convenience — call these once per frame from the main loop in order:
    //    1. update_frame()   — refresh caches
    //    2. RenderXxx(state) — each panel
    //    3. process_events() — apply deferred side-effects
    //    4. tick(now)        — advance VM timer
    // =========================================================================
    void update_frame();
};
