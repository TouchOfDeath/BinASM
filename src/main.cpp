// =============================================================================
// main.cpp  —  Educational Binary IDE  (Cosmic Aurora edition)
//
// Responsibilities of this file (Phase 4):
//   • GLFW + ImGui initialisation and teardown.
//   • One-time DockBuilder layout.
//   • Per-frame orchestration: update_frame → panels → process_events → tick.
//
// All application state lives in AppState (app_state.h).
// All panel rendering lives in panel_*.h / panel_*.cpp.
// All VM simulation is driven through the AppEvent queue.
// =============================================================================

#include <cstdio>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "app_state.h"
#include "fonts.h"
#include "ui_premium.h"

// Extracted panel modules.
#include "panel_editor.h"
#include "panel_explorer.h"
#include "panel_vm_state.h"
#include "panel_console.h"
#include "panel_debug_assist.h"
#include "panel_disasm.h"
#include "panel_decompiled.h"
#include "panel_macro.h"

// Phase-3 panels (smaller, no AppState dependency).
#include "panel_cfg.h"
#include "panel_statusbar.h"
#include "panel_cheatsheet.h"

// Hex editor rendered directly (it writes back into vm.memory).
#include "hex_editor.h"

// New features: Memory Grid and Step-Back System
#include "memory_grid.h"
#include "step_back.h"

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // macOS requires a Core Profile context at 3.2+ and GLSL 150.
    // Linux and Windows work fine with a compatibility context at 3.0 + GLSL 130.
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1680, 980,
        "Educational Binary IDE  —  Cosmic Aurora", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // disable layout auto-save (we own the layout)

    LoadFonts(io);
    ui::icons::load(io);
    // NOTE: do NOT call io.Fonts->Build() here.  The docking branch sets
    // ImGuiBackendFlags_RendererHasTextures inside ImGui_ImplOpenGL3_Init(),
    // and calling Build() before that flag exists triggers an assertion.
    // The backend drives atlas building automatically on first NewFrame().

    ui::SetupPremiumTheme();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] =
        ui::col::v(ui::col::with_alpha(ui::col::SURFACE, 0.97f));

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // =========================================================================
    //  Application state
    // =========================================================================
    AppState state;
    state.init();

    // =========================================================================
    //  Main render loop
    // =========================================================================
    while (!glfwWindowShouldClose(window)) {
        // --- Event-driven rendering ------------------------------------------
        // When the VM is idle, sleep up to 33 ms between frames (30 fps cap).
        // When the VM is running we need ~2 Hz ticks so limit sleep to 100 ms.
        // Any GLFW event (key, mouse move, resize, …) wakes us immediately,
        // so interactive responsiveness is unaffected.
        {
            double timeout = (state.vm_running && !state.vm.halted) ? 0.10 : 0.033;
            glfwWaitEventsTimeout(timeout);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ui::anim::update();

        // ---- Dockspace with aurora background -------------------------------
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ui::draw_dockspace_background(vp->Pos, vp->Size);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
            0, vp, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::PopStyleVar();

        // ---- One-time DockBuilder layout ------------------------------------
        // Runs AFTER DockSpaceOverViewport (which creates the live node).
        // Running it before destroys the live node → zero-size rect → invisible
        // panels. The vp->Size guard skips the first frame if GLFW hasn't
        // reported a real size yet.
        static bool first_time = true;
        if (first_time && vp->Size.x > 100.0f) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->Size);

            ImGuiID dock_main   = dockspace_id;
            ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down,  0.22f, nullptr, &dock_main);
            ImGuiID dock_left   = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left,  0.14f, nullptr, &dock_main);
            ImGuiID dock_vm     = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left,  0.16f, nullptr, &dock_main);
            ImGuiID dock_right  = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.35f, nullptr, &dock_main);

            ImGui::DockBuilderDockWindow("Explorer",             dock_left);
            ImGui::DockBuilderDockWindow("VM State (8-bit CPU)", dock_vm);
            ImGui::DockBuilderDockWindow("Opcode Cheat Sheet",   dock_vm);
            ImGui::DockBuilderDockWindow("Console Output",       dock_bottom);
            ImGui::DockBuilderDockWindow("Debug Assistant",      dock_bottom);
            ImGui::DockBuilderDockWindow("Decompiled Output",    dock_right);
            ImGui::DockBuilderDockWindow("Control Flow Graph",   dock_right);
            ImGui::DockBuilderDockWindow("Memory Hex Editor",    dock_right);
            ImGui::DockBuilderDockWindow("Macro Inspector",      dock_right);
            ImGui::DockBuilderDockWindow("Binary Editor",        dock_main);
            ImGui::DockBuilderDockWindow("Live Disassembler",    dock_main);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        // =========================================================================
        //  Global keyboard shortcuts (processed once per frame, before panels).
        //  These fire regardless of whether a text widget has focus so that
        //  IDE hotkeys work while typing in the code editor (matching VS Code).
        // =========================================================================
        {
            ImGuiIO& io = ImGui::GetIO();
            const bool ctrl = io.KeyCtrl;

            // Ctrl+S  — Save active tab
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
                state.push_event(AppEventKind::Save);
            }
            // Ctrl+O  — Open file (modal)
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
                state.open_file_dialog_open = true;
                state.open_file_input[0] = '\0';
            }
            // F5 — Run/Continue
            if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
                state.push_event(AppEventKind::Run);
            }
            // F10 — Step Over (same as Step on this flat-memory VM)
            if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) {
                state.push_event(AppEventKind::Step);
            }
            // F11 — Step Into
            if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
                state.push_event(AppEventKind::Step);
            }
        }

        // =========================================================================
        //  Ctrl+O — Open File modal (rendered at top level so it can be
        //  opened from anywhere, e.g. the keyboard shortcut above).
        // =========================================================================
        if (state.open_file_dialog_open) {
            ImGui::OpenPopup("Open File##ctrl_o");
            state.open_file_dialog_open = false;
        }
        ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Open File##ctrl_o", nullptr,
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextDisabled("Enter a file path to open (relative to the working directory):");
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::SetNextItemWidth(-1.0f);
            bool enter = ImGui::InputText("##open_path", state.open_file_input,
                                          sizeof(state.open_file_input),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SetItemDefaultFocus();
            ImGui::Dummy(ImVec2(0, 4));
            float btn_w = 90.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX(
                (ImGui::GetContentRegionAvail().x - btn_w * 2 - spacing) * 0.5f +
                 ImGui::GetStyle().WindowPadding.x);
            bool open_clicked = ImGui::Button("Open", ImVec2(btn_w, 0));
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
                ImGui::CloseCurrentPopup();
            }
            if (enter || open_clicked) {
                if (state.open_file_input[0] != '\0') {
                    state.push_event(AppEventKind::OpenFile,
                                     std::string(state.open_file_input));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::EndPopup();
        }

        // =========================================================================
        //  Frame update — refresh caches (lint / disasm / macros / decompiler).
        //  Must run before panels so every panel reads current derived state.
        // =========================================================================
        state.update_frame();

        // =========================================================================
        //  Panels — order does not affect correctness; events are deferred.
        // =========================================================================
        RenderExplorer(state);
        RenderBinaryEditor(state);
        RenderVMState(state);
        RenderLiveDisassembler(state);
        RenderDecompiledOutput(state);
        RenderMacroInspector(state);
        RenderConsole(state);
        RenderDebugAssistant(state);
        RenderCheatSheetPanel();

        // Hex editor writes directly into vm.memory and signals dirty.
        {
            uint8_t pc_highlight = state.vm.halted ? 0xFFu : state.vm.pc;
            if (state.hex_editor.render("Memory Hex Editor",
                                        state.vm.memory, 16, pc_highlight)) {
                state.decompiler_dirty = true;
            }
        }

        // Memory Grid Inspector - new feature with color-coded visualization
        {
            if (get_memory_grid().render("Memory Grid Inspector", state.vm)) {
                state.decompiler_dirty = true;
            }
        }

        // CFG and status bar read from state members directly (Phase-3 API).
        RenderCFGWindow("Control Flow Graph",
                        state.decompiler,
                        state.vm_running && !state.vm.halted);

        RenderStatusBar(state.vm_running, state.vm.halted,
                        state.vm, state.has_errors);

        // =========================================================================
        //  Event dispatch — consume everything panels pushed this frame.
        // =========================================================================
        state.process_events();

        // =========================================================================
        //  VM timing — advance the simulation at ~2 Hz when running.
        // =========================================================================
        state.tick(glfwGetTime());

        // =========================================================================
        //  Render
        // =========================================================================
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.039f, 0.055f, 0.102f, 1.0f);  // #0A0E1A — aurora BG
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        GLFWwindow* backup_ctx = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_ctx);

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
