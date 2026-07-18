// =============================================================================
// panel_explorer.cpp  —  Workspace file explorer sidebar.
// =============================================================================

#include "panel_explorer.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

void RenderExplorer(AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

    ui::header_gradient("EXPLORER", ui::col::ACCENT_CYAN);
    ImGui::Dummy(ImVec2(0, 2));

    if (ImGui::SmallButton(" Refresh ")) state.explorer_needs_refresh = true;
    ImGui::SameLine();
    if (ImGui::SmallButton(" + New "))   state.push_event(AppEventKind::NewTab);

    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.20f);
    ImGui::Dummy(ImVec2(0, 4));

    // ---- Open Editors section -----------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_SEC));
    ImGui::TextUnformatted("  OPEN EDITORS");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2));

    for (int i = 0; i < (int)state.tabs.size(); ++i) {
        std::string label = (state.tabs[i].dirty ? "* " : "  ") + state.tabs[i].name;
        bool selected = (i == state.active_tab);
        ImGui::PushID(i + 1000);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Header,
                ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.20f)));
        }
        if (ImGui::Selectable(label.c_str(), selected,
                               ImGuiSelectableFlags_None, ImVec2(-1, 22))) {
            state.active_tab = i;
        }
        if (ImGui::BeginPopupContextItem("tab_ctx")) {
            if (ImGui::MenuItem("Close")) {
                state.push_event(AppEventKind::CloseTab, {}, i);
            }
            ImGui::EndPopup();
        }
        if (selected) ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 8));

    // ---- Workspace Files section --------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_SEC));
    ImGui::TextUnformatted("  WORKSPACE FILES");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 2));

    if (state.explorer_files.empty()) {
        ImGui::TextDisabled("  (no .bincode files found)");
    }
    for (const auto& fname : state.explorer_files) {
        std::string label = "  " + fname;
        if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(-1, 20))) {
            state.push_event(AppEventKind::OpenFile, fname);
        }
    }

    ImGui::PopStyleVar(); // inner WindowPadding
    ImGui::End();
    ImGui::PopStyleVar(); // outer WindowPadding=0
}
