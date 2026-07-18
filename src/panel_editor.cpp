// =============================================================================
// panel_editor.cpp  —  Tabbed binary source editor panel.
// =============================================================================

#include "panel_editor.h"
#include "app_state.h"
#include "fonts.h"
#include "ui_premium.h"
#include "hover_provider.h"

#include "imgui.h"
#include "imgui_internal.h"

void RenderBinaryEditor(AppState& state) {
    // ---- Save-error modal (must be opened/checked before Begin/End pairs) ---
    if (state.save_error_open) {
        ImGui::OpenPopup("Save Error##modal");
        state.save_error_open = false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::Begin("Binary Editor");

    // =========================================================================
    //  VS Code-style reorderable tab bar
    // =========================================================================
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Tab,
        ui::col::v(ui::col::with_alpha(ui::col::BG_MID, 0.8f)));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,
        ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.20f)));
    ImGui::PushStyleColor(ImGuiCol_TabActive,
        ui::col::v(ui::col::SURFACE_HI));

    if (ImGui::BeginTabBar("##editor_tabs",
                           ImGuiTabBarFlags_Reorderable       |
                           ImGuiTabBarFlags_AutoSelectNewTabs  |
                           ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < (int)state.tabs.size(); ++i) {
            std::string label =
                (state.tabs[i].dirty ? "* " : "") +
                state.tabs[i].name + "###tab" + std::to_string(i);
            bool tab_open = true;
            bool closeable = state.tabs.size() > 1;
            if (ImGui::BeginTabItem(label.c_str(), closeable ? &tab_open : nullptr)) {
                state.active_tab = i;
                ImGui::EndTabItem();
            }
            if (!tab_open && closeable) {
                state.push_event(AppEventKind::CloseTab, {}, i);
                break;
            }
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            state.push_event(AppEventKind::NewTab);
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0, 2));

    // =========================================================================
    //  Premium toolbar
    // =========================================================================
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));

    if (ui::icon_button(ui::icons::play(), "Run",
                        "Run VM  [F5]\nAssemble source and start execution from the top.",
                        ImVec2(44, 34), ui::col::ACCENT_GREEN)) {
        state.push_event(AppEventKind::Run);
    }
    ImGui::SameLine();
    if (ui::icon_button(ui::icons::step(), "Step",
                        "Step VM  [F10 / F11]\nExecute exactly one instruction, then pause.",
                        ImVec2(44, 34), ui::col::ACCENT_CYAN)) {
        state.push_event(AppEventKind::Step);
    }
    ImGui::SameLine();
    if (ui::icon_button(ui::icons::reset(), "Reset",
                        "Reset VM\nClear all registers, flags, and console output.",
                        ImVec2(44, 34), ui::col::ACCENT_AMBER)) {
        state.push_event(AppEventKind::Reset);
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(4, 0));
    ImGui::SameLine();
    // Visual separator between action and file-op groups.
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 sep_pos  = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sep_pos.x, sep_pos.y + 4),
                    ImVec2(sep_pos.x, sep_pos.y + 28),
                    ui::col::with_alpha(0xFFFFFFFF, 0.10f), 1.0f);
    }
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();

    if (ui::icon_button(ui::icons::save(), "Save",
                        "Save  [Ctrl+S]\nWrite active tab to disk.",
                        ImVec2(38, 34), ui::col::TEXT_SEC)) {
        state.push_event(AppEventKind::Save);
    }
    ImGui::SameLine();
    if (ui::icon_button(ui::icons::load_icon(), "Load",
                        "Load  —  read source from workspace.bincode\n"
                        "Use Ctrl+O to open any file by path.",
                        ImVec2(38, 34), ui::col::TEXT_SEC)) {
        state.push_event(AppEventKind::Load);
    }
    ImGui::SameLine();
    if (ui::icon_button(ui::icons::decompile(), "Decompile",
                        "Decompile Memory\nReverse-engineer the current memory state into source.",
                        ImVec2(38, 34), ui::col::ACCENT_MAG)) {
        state.push_event(AppEventKind::Decompile);
    }

    // ---- Error banner -------------------------------------------------------
    // Show a prominent error count when the assembler finds problems,
    // so users get immediate feedback without having to look at the
    // Debug Assistant panel.
    if (state.has_errors) {
        ImGui::SameLine(0, 16);
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_CORAL));
        ImGui::TextUnformatted(ui::icons::err());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_CORAL),
                           "%d error(s)  —  see Debug Assistant",
                           (int)state.error_markers.size());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_CORAL),
                               "Assembler errors:");
            for (const auto& [line, msg] : state.error_markers) {
                ImGui::TextDisabled("  Line %d:", line);
                ImGui::SameLine();
                ImGui::TextUnformatted(msg.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    ImGui::PopStyleVar(); // ItemSpacing

    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.25f);
    ImGui::Dummy(ImVec2(0, 2));

    // =========================================================================
    //  TextEditor widget
    // =========================================================================
    if (g_editor_font) ImGui::PushFont(g_editor_font);
    state.tabs[state.active_tab].editor.Render("TextEditor");
    if (g_editor_font) ImGui::PopFont();

    // =========================================================================
    //  Hover IntelliSense Tooltip
    // =========================================================================
    // Render hover tooltip after the editor, using the global hover system
    {
        const auto& hover_system = get_hover_system();
        HoverContext ctx = hover_system.detect_hover(state.tabs[state.active_tab].editor);
        hover_system.render_tooltip(ctx, state);
    }

    ImGui::End();
    ImGui::PopStyleVar(); // WindowPadding

    // =========================================================================
    //  Save-error modal
    // =========================================================================
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Save Error##modal", nullptr,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_CORAL));
        ImGui::TextUnformatted(ui::icons::err());
        ImGui::SameLine();
        ImGui::TextUnformatted(" Failed to save file");
        ImGui::PopStyleColor();
        ui::separator_gradient(ui::col::ACCENT_CORAL, 0.35f);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextDisabled("Path:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", state.save_error_path.c_str());
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_AMBER));
        ImGui::TextWrapped("%s", state.save_error_msg.c_str());
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 8));
        float btn_w = 100.0f;
        ImGui::SetCursorPosX(
            (ImGui::GetContentRegionAvail().x - btn_w) * 0.5f +
             ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("OK", ImVec2(btn_w, 0))) ImGui::CloseCurrentPopup();
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::EndPopup();
    }
}
