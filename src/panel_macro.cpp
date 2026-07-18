// =============================================================================
// panel_macro.cpp  —  Pass-0 macro inspector panel.
//
// Reads MacroCache (pre-computed by AppState::update_macros()) and renders
// a table of defined macros plus a collapsible expanded-source view.
// =============================================================================

#include "panel_macro.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

#include <string>

void RenderMacroInspector(const AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::Begin("Macro Inspector");

    ui::header_gradient("Preprocessor Macros", ui::col::ACCENT_PUR);
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::TextDisabled("Pass-0 state.  Define macros with  #define NAME(args) body");
    ui::separator_gradient(ui::col::ACCENT_PUR, 0.25f);

    if (!state.macro_cache.valid) {
        ImGui::TextDisabled("Run or Step the VM to see preprocessor output.");
    } else if (state.macro_cache.macros.empty()) {
        ImGui::TextDisabled("No #define macros in current source.");
    } else {
        if (ImGui::BeginTable("macros", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Params");
            ImGui::TableSetupColumn("Body");
            ImGui::TableSetupColumn("Line");
            ImGui::TableHeadersRow();

            for (const auto& m : state.macro_cache.macros) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ui::col::v(ui::col::ACCENT_PUR),
                                   "%s", m.name.c_str());

                ImGui::TableSetColumnIndex(1);
                if (m.params.empty()) {
                    ImGui::TextDisabled("(none)");
                } else {
                    std::string p;
                    for (size_t i = 0; i < m.params.size(); ++i) {
                        if (i) p += ", ";
                        p += m.params[i];
                    }
                    ImGui::TextUnformatted(p.c_str());
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", m.body.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::TextDisabled("%d", m.definition_line);
            }
            ImGui::EndTable();
        }
    }

    // Expanded-source collapsible section (always available after first lint).
    if (!state.macro_cache.expanded.empty()) {
        ui::separator_gradient(ui::col::ACCENT_PUR, 0.20f);
        if (ImGui::CollapsingHeader("Expanded Source (Post-Pass-0)")) {
            // Mutable copy for the same ReadOnly-InputTextMultiline reason.
            std::string expanded_copy = state.macro_cache.expanded;
            ImGui::PushStyleColor(ImGuiCol_FrameBg,
                ui::col::v(ui::col::with_alpha(0xFF0E0F1A, 0.85f)));
            ImGui::InputTextMultiline("##expanded",
                expanded_copy.data(), expanded_copy.size() + 1,
                ImVec2(-1.0f, 200.0f), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
