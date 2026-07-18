// =============================================================================
// panel_console.cpp  —  VM output stream console panel.
// =============================================================================

#include "panel_console.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

void RenderConsole(const AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("Console Output");

    ui::header_gradient("VM Output Stream", ui::col::ACCENT_GREEN);
    ImGui::Dummy(ImVec2(0, 6));
    ui::separator_gradient(ui::col::ACCENT_GREEN, 0.25f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ui::col::v(ui::col::with_alpha(0xFF0E0F1A, 0.50f)));
    ImGui::BeginChild("##console", ImVec2(-1, -1), true);

    if (state.vm.console_output.empty()) {
        ImGui::TextDisabled("(no output yet  —  run the VM)");
    } else {
        for (const auto& log : state.vm.console_output) {
            ImGui::TextDisabled("> ");
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_GREEN), "%s", log.c_str());
        }
        // Auto-scroll to bottom.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar();
}
