// =============================================================================
// panel_debug_assist.cpp  —  Live lint diagnostics assistant panel.
// =============================================================================

#include "panel_debug_assist.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

void RenderDebugAssistant(const AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("Debug Assistant");

    ui::header_gradient("Diagnostics", ui::col::ACCENT_AMBER);
    ImGui::Dummy(ImVec2(0, 6));
    ui::separator_gradient(ui::col::ACCENT_AMBER, 0.25f);

    if (state.warnings.empty() && !state.has_errors) {
        ui::status_dot(ui::col::ACCENT_GREEN, 5.0f, false);
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_GREEN),
                           "Looks good! No logical errors detected.");
    } else {
        if (state.has_errors) {
            ui::status_dot(ui::col::ACCENT_CORAL, 5.0f, true);
            ImGui::SameLine();
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_CORAL),
                               "%d error(s) detected in source.",
                               (int)state.error_markers.size());
        }
        for (const auto& warn : state.warnings) {
            ui::status_dot(ui::col::ACCENT_AMBER, 4.0f, false);
            ImGui::SameLine();
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_AMBER),
                               "%s", warn.message.c_str());
            ui::separator_gradient(ui::col::ACCENT_AMBER, 0.10f);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
