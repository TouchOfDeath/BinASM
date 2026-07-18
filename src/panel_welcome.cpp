// =============================================================================
// panel_welcome.cpp  —  Welcome dashboard and onboarding system
// =============================================================================
// This module provides a friendly first-run experience for new users:
//   • Welcome Dashboard with big action buttons (New Project, Open Example)
//   • Hidden panels by default (shown only when needed)
//   • Interactive tutorials with overlay instructions
//   • Layout presets (Beginner/Debugger/Architect modes)
// =============================================================================

#include "panel_welcome.h"
#include "app_state.h"
#include "ui_premium.h"
#include "fonts.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>

namespace {

// Tutorial step data structure
struct TutorialStep {
    const char* title;
    const char* description;
    const char* highlight_panel;  // Panel name to highlight (empty = none)
    const char* code_hint;        // Code snippet or hint to show
};

// Built-in tutorial steps for first-time users
const std::vector<TutorialStep> g_tutorial_steps = {
    {
        "Welcome to Binary IDE!",
        "This is your educational environment for learning assembly programming. "
        "Let's take a quick tour of the interface.",
        "",
        ""
    },
    {
        "The Editor",
        "This is where you write your assembly code. The editor supports syntax highlighting, "
        "auto-indentation, and hover tooltips for opcodes.",
        "Binary Editor",
        "LDA 0x10  ; Load from memory address 0x10"
    },
    {
        "Running Code",
        "Use the Run button (or press F5) to execute your program. "
        "Watch the VM State panel to see registers and flags update in real-time.",
        "",
        "Press F5 to run"
    },
    {
        "Debugging",
        "Step through code one instruction at a time with F10/F11. "
        "The Debug Assistant shows errors and warnings as you type.",
        "Debug Assistant",
        ""
    },
    {
        "Memory View",
        "The Memory Grid and Hex Editor show the raw contents of VM memory. "
        "You can edit memory directly in hex mode!",
        "Memory Grid Inspector",
        ""
    },
    {
        "You're Ready!",
        "Start coding! Remember: you can always access help from the Opcode Cheat Sheet "
        "or revisit this tutorial from the Help menu.",
        "",
        "Happy Coding!"
    }
};

// Render a single tutorial step card
void renderTutorialCard(const TutorialStep& step, int currentStep, int totalSteps) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    
    // Title with accent
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_CYAN));
    ImGui::TextUnformatted(step.title);
    ImGui::PopStyleColor();
    
    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.3f);
    ImGui::Dummy(ImVec2(0, 8));
    
    // Description
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 40);
    ImGui::TextUnformatted(step.description);
    ImGui::PopTextWrapPos();
    
    ImGui::Dummy(ImVec2(0, 12));
    
    // Code hint if available
    if (step.code_hint[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::col::v(ui::col::with_alpha(ui::col::BG_MID, 0.5f)));
        ImGui::BeginChild("##code_hint", ImVec2(-1, 60), false, ImGuiWindowFlags_NoDecoration);
        if (g_editor_font) ImGui::PushFont(g_editor_font);
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_GREEN));
        ImGui::TextUnformatted(step.code_hint);
        if (g_editor_font) ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 8));
    }
    
    // Progress indicator
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Step %d of %d", currentStep + 1, totalSteps);
    ImGui::SameLine();
    
    // Progress dots
    for (int i = 0; i < totalSteps; ++i) {
        ImGui::SameLine(0, 4);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 center = ImGui::GetCursorScreenPos();
        float radius = (i == currentStep) ? 5.0f : 3.0f;
        ImU32 color = (i <= currentStep) ? 
            ui::col::ACCENT_CYAN : 
            ui::col::TEXT_DIS;
        dl->AddCircleFilled(center, radius, color);
    }
    
    ImGui::PopStyleVar();
}

} // anonymous namespace

void RenderWelcomeDashboard(AppState& state) {
    // Check if we should show welcome (first run or explicitly requested)
    if (!state.show_welcome) return;
    
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGui::Begin("##WelcomeDashboard", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // Background gradient
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bg_start = vp->Pos;
    ImVec2 bg_end = ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y);
    dl->AddRectFilledMultiColor(
        bg_start, bg_end,
        ui::col::BG_DEEP,
        ui::col::BG_DEEP,
        ui::col::BG_MID,
        ui::col::BG_MID
    );
    
    // Main content area
    float content_width = 900.0f;
    float content_height = 600.0f;
    float content_x = vp->Pos.x + (vp->Size.x - content_width) * 0.5f;
    float content_y = vp->Pos.y + (vp->Size.y - content_height) * 0.5f;
    
    // Card background
    ImVec2 card_min = ImVec2(content_x, content_y);
    ImVec2 card_max = ImVec2(content_x + content_width, content_y + content_height);
    dl->AddRectFilled(card_min, card_max, ui::col::with_alpha(ui::col::SURFACE, 0.95f), 12.0f);
    dl->AddRect(card_min, card_max, ui::col::BORDER_SUB, 12.0f);
    
    // Content container
    ImGui::SetCursorPos(ImVec2(content_x + 40, content_y + 40));
    ImGui::BeginGroup();
    
    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_PRI));
    ImGui::TextUnformatted("Welcome to Binary IDE Premium");
    ImGui::PopStyleColor();
    
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_SEC));
    ImGui::TextUnformatted("Your educational environment for 8-bit assembly programming");
    ImGui::PopStyleColor();
    
    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.4f);
    ImGui::Dummy(ImVec2(0, 20));
    
    // Quick actions grid
    ImGui::TextUnformatted("Get Started:");
    ImGui::Dummy(ImVec2(0, 12));
    
    float btn_width = 200.0f;
    float btn_height = 80.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    
    // Row 1: New Project, Open Example
    if (ui::gradient_button((std::string(ui::icons::code()) + "  New Project").c_str(),
                            ImVec2(btn_width, btn_height),
                            ui::col::ACCENT_CYAN, ui::col::ACCENT_PUR)) {
        state.push_event(AppEventKind::NewTab);
        state.show_welcome = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Create a new blank assembly file");
        ImGui::EndTooltip();
    }
    
    ImGui::SameLine(0, spacing * 2);
    
    if (ui::gradient_button((std::string(ui::icons::load_icon()) + "  Open Example").c_str(),
                            ImVec2(btn_width, btn_height),
                            ui::col::ACCENT_MAG, ui::col::ACCENT_PUR)) {
        // TODO: Load example project
        state.show_welcome = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Load a sample program to explore");
        ImGui::EndTooltip();
    }
    
    // Row 2: Continue Tutorial, Load Recent
    ImGui::Dummy(ImVec2(0, spacing));
    
    if (ui::gradient_button((std::string(ui::icons::info()) + "  Start Tutorial").c_str(),
                            ImVec2(btn_width, btn_height),
                            ui::col::ACCENT_GREEN, ui::col::ACCENT_CYAN)) {
        state.show_tutorial = true;
        state.tutorial_step = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Interactive walkthrough of the IDE");
        ImGui::EndTooltip();
    }
    
    ImGui::SameLine(0, spacing * 2);
    
    // Recent files (placeholder)
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Recent Files:");
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_DIS));
    ImGui::TextUnformatted("(none yet)");
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    
    ImGui::Dummy(ImVec2(0, 24));
    
    // Tips section
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::col::v(ui::col::with_alpha(ui::col::BG_MID, 0.3f)));
    ImGui::BeginChild("##tips", ImVec2(-1, 100), false, ImGuiWindowFlags_NoDecoration);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_AMBER));
    ImGui::TextUnformatted(ui::icons::bolt());
    ImGui::SameLine();
    ImGui::TextUnformatted("Quick Tip:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 30);
    ImGui::TextUnformatted("Hover over any opcode in the editor to see detailed documentation. "
                          "Try hovering over LDA, ADD, or JMP!");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // Bottom actions
    ImGui::Dummy(ImVec2(0, 16));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));
    
    bool dont_show_again = state.dont_show_welcome;
    if (ImGui::Checkbox("Don't show this on startup", &dont_show_again)) {
        state.dont_show_welcome = dont_show_again;
    }
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 100 + ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button("Skip to Editor", ImVec2(100, 0))) {
        state.show_welcome = false;
    }
    
    ImGui::EndGroup();
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void RenderOnboardingOverlay(AppState& state) {
    if (!state.show_tutorial) return;
    
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 50, vp->Pos.y + 50));
    ImGui::SetNextWindowSize(ImVec2(500, 400));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::col::v(ui::col::ACCENT_CYAN));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::col::v(ui::col::with_alpha(ui::col::SURFACE, 0.98f)));
    
    ImGui::Begin("##TutorialOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);
    
    const auto& step = g_tutorial_steps[state.tutorial_step];
    renderTutorialCard(step, state.tutorial_step, (int)g_tutorial_steps.size());
    
    ImGui::Dummy(ImVec2(0, 20));
    
    // Navigation buttons
    float btn_w = 100.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    float total_btn_w = btn_w * 2 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX((avail_w - total_btn_w) * 0.5f);
    
    bool prev_clicked = ImGui::Button("Back", ImVec2(btn_w, 0));
    ImGui::SameLine();
    bool next_clicked = ImGui::Button(state.tutorial_step == (int)g_tutorial_steps.size() - 1 ? 
                                      "Finish" : "Next", ImVec2(btn_w, 0));
    
    if (prev_clicked && state.tutorial_step > 0) {
        state.tutorial_step--;
    }
    
    if (next_clicked) {
        if (state.tutorial_step < (int)g_tutorial_steps.size() - 1) {
            state.tutorial_step++;
        } else {
            state.show_tutorial = false;
        }
    }
    
    // Close button
    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_w - 40);
    if (ImGui::SmallButton("X")) {
        state.show_tutorial = false;
    }
    
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}
