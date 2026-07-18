// =============================================================================
// app_state.cpp  —  AppState lifecycle, event dispatch, cache management.
// =============================================================================

#include "app_state.h"
#include "disassembler.h"
#include "editor_tab.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

// =============================================================================
//  Lifecycle
// =============================================================================

void AppState::init() {
    tabs.push_back(make_editor_tab(
        "program.bincode", "",
        "# Define a macro that adds 10 to a memory cell.\n"
        "#define ADD_TEN(x) LDA x; ADD 10d; STA x\n"
        "\n"
        "start:\n"
        "    LDI 0\n"
        "    STA 15  # counter = 0\n"
        "loop:\n"
        "    LDA 15\n"
        "    OUT\n"
        "    ADD_TEN(15)\n"
        "    LDA 15\n"
        "    SUB 11  # compare against 5d via 11d (5+6=11)\n"
        "    JC done\n"
        "    JMP loop\n"
        "done:\n"
        "    HLT\n"
    ));
}

void AppState::tick(double now) {
    if (!vm_running || vm.halted) {
        if (vm.halted) vm_running = false;
        return;
    }
    if (now - last_step_time > 0.5) {
        vm.step();
        last_step_time   = now;
        decompiler_dirty = true;

        // Cap console output to prevent unbounded memory growth on long-running
        // or infinite-loop programs.  Keep the most recent 500 lines.
        constexpr std::size_t kMaxConsoleLines = 500;
        if (vm.console_output.size() > kMaxConsoleLines) {
            vm.console_output.erase(
                vm.console_output.begin(),
                vm.console_output.begin() +
                    static_cast<std::ptrdiff_t>(vm.console_output.size() - kMaxConsoleLines));
        }
    }
}

// =============================================================================
//  Event queue
// =============================================================================

void AppState::push_event(AppEventKind kind, std::string payload, int tab_index) {
    events.emplace_back(kind, std::move(payload), tab_index);
}

void AppState::process_events() {
    for (auto& ev : events) {
        switch (ev.kind) {

        case AppEventKind::Run: {
            const std::string& src = tabs[active_tab].editor.GetText();
            vm.loadProgram(src);
            vm_running       = true;
            first_load       = false;
            last_step_time   = 0.0;  // tick() will step immediately on next frame
            decompiler_dirty = true;
            // Invalidate analysis caches so lint / disasm re-run on new source.
            lint_cache.valid  = false;
            disasm_cache.valid = false;
            macro_cache.valid  = false;
            break;
        }

        case AppEventKind::Step: {
            if (first_load) {
                const std::string& src = tabs[active_tab].editor.GetText();
                vm.loadProgram(src);
                first_load = false;
            }
            vm.step();
            vm_running       = true;
            decompiler_dirty = true;
            break;
        }

        case AppEventKind::Reset:
            vm.reset();
            vm_running       = false;
            decompiler_dirty = true;
            break;

        case AppEventKind::Decompile:
            decompiler_dirty = true;
            break;

        case AppEventKind::Save: {
            int idx = (ev.tab_index < 0) ? active_tab : ev.tab_index;
            if (idx < 0 || idx >= (int)tabs.size()) break;
            std::string& path = ev.payload.empty() ? tabs[idx].filepath : ev.payload;
            if (path.empty()) path = tabs[idx].name;
            std::ofstream out(path);
            if (out.is_open()) {
                out << tabs[idx].editor.GetText();
                out.close();
                if (out.good()) {
                    tabs[idx].dirty        = false;
                    explorer_needs_refresh = true;
                } else {
                    save_error_path = path;
                    save_error_msg  = "Write failed (disk full or I/O error).";
                    save_error_open = true;
                }
            } else {
                save_error_path = path;
                save_error_msg  = "Could not open file for writing.\n"
                                  "Check that the directory exists and you have write permission.";
                save_error_open = true;
            }
            break;
        }

        case AppEventKind::Load: {
            const std::string& path = ev.payload.empty() ? "workspace.bincode" : ev.payload;
            std::ifstream in(path);
            if (in.is_open()) {
                std::stringstream buf;
                buf << in.rdbuf();
                tabs[active_tab].editor.SetText(buf.str());
                tabs[active_tab].dirty = false;
                decompiler_dirty       = true;
                lint_cache.valid       = false;
                disasm_cache.valid     = false;
                macro_cache.valid      = false;
            }
            break;
        }

        case AppEventKind::NewTab: {
            char name[32];
            std::snprintf(name, sizeof(name), "untitled_%d.bincode", (int)tabs.size() + 1);
            tabs.push_back(make_editor_tab(name, "", "# New program\nHLT\n"));
            active_tab = (int)tabs.size() - 1;
            break;
        }

        case AppEventKind::CloseTab: {
            int idx = (ev.tab_index < 0) ? active_tab : ev.tab_index;
            if (tabs.size() <= 1) break;
            tabs.erase(tabs.begin() + idx);
            if (active_tab >= (int)tabs.size()) active_tab = (int)tabs.size() - 1;
            lint_cache.valid  = false;
            disasm_cache.valid = false;
            macro_cache.valid  = false;
            break;
        }

        case AppEventKind::OpenFile: {
            const std::string& path = ev.payload;
            if (path.empty()) break;
            // Switch to the tab if it is already open.
            bool already_open = false;
            for (int i = 0; i < (int)tabs.size(); ++i) {
                if (tabs[i].filepath == path) {
                    active_tab = i;
                    already_open = true;
                    break;
                }
            }
            if (!already_open) {
                std::ifstream in(path);
                if (in.is_open()) {
                    std::stringstream buf;
                    buf << in.rdbuf();
                    std::string fname = std::filesystem::path(path).filename().string();
                    tabs.push_back(make_editor_tab(fname, path, buf.str()));
                    active_tab = (int)tabs.size() - 1;
                    lint_cache.valid   = false;
                    disasm_cache.valid = false;
                    macro_cache.valid  = false;
                }
            }
            break;
        }

        } // switch
    }
    events.clear();
}

// =============================================================================
//  Cache updates
// =============================================================================

void AppState::update_lint(const std::string& text) {
    if (lint_cache.valid && lint_cache.text == text) {
        // Already current — just rebuild derived state in case nothing changed.
    } else {
        lint_cache.text  = text;
        lint_cache.diags = vm.analyzeProgram(text);
        lint_cache.valid = true;
    }

    // Rebuild derived error_markers / warnings / has_errors.
    error_markers.clear();
    warnings.clear();
    has_errors = false;

    for (const auto& diag : lint_cache.diags) {
        if (diag.is_error && diag.line_number > 0) {
            error_markers[diag.line_number] = diag.message;
            has_errors = true;
        } else if (!diag.is_error) {
            warnings.push_back(diag);
        }
    }

    // Push markers into the active editor so red squiggles appear.
    if (!tabs.empty()) {
        tabs[active_tab].editor.SetErrorMarkers(error_markers);
    }
}

void AppState::update_disasm(const std::string& text) {
    if (disasm_cache.valid && disasm_cache.text == text) return;
    disasm_cache.text   = text;
    disasm_cache.result = disassembleBinary(text);
    disasm_cache.valid  = true;
}

void AppState::update_macros(const std::string& text) {
    if (macro_cache.valid && macro_cache.text == text) return;
    macro_cache.text  = text;
    Preprocessor pp;
    PreprocessedSource ps = pp.process(text);
    macro_cache.macros   = pp.lastMacros();
    macro_cache.expanded = ps.text;
    macro_cache.valid    = true;
}

void AppState::refresh_explorer() {
    explorer_files.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && !ec) {
            if (entry.path().extension() == ".bincode") {
                explorer_files.push_back(entry.path().filename().string());
            }
        }
        ec.clear();
    }
    std::sort(explorer_files.begin(), explorer_files.end());
    explorer_needs_refresh = false;
}

// =============================================================================
//  update_frame — convenience: refresh all caches for the active tab.
//  Call once per frame BEFORE rendering panels.
// =============================================================================
void AppState::update_frame() {
    if (tabs.empty()) return;

    // Decompiler (only runs when dirty — avoids full pipeline at 60 fps).
    if (decompiler_dirty) {
        decompiler.decompile(vm.memory);
        decompiler_dirty = false;
    }

    if (explorer_needs_refresh) refresh_explorer();

    const std::string cur_text = tabs[active_tab].editor.GetText();

    // Update dirty flag from TextEditor's internal change tracker.
    if (tabs[active_tab].editor.IsTextChanged()) {
        tabs[active_tab].dirty = true;
    }

    update_lint(cur_text);
    update_disasm(cur_text);
    update_macros(cur_text);
}
