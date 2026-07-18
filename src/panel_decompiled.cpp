// =============================================================================
// panel_decompiled.cpp  —  Reverse-engineered source output panel.
// =============================================================================

#include "panel_decompiled.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

void RenderDecompiledOutput(AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("Decompiled Output");

    ui::header_gradient("Reverse-Engineered Source", ui::col::ACCENT_MAG);
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::TextDisabled("Runs on Run / Step / Hex Editor edit.  Manual refresh:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        state.push_event(AppEventKind::Decompile);
    }
    ui::separator_gradient(ui::col::ACCENT_MAG, 0.25f);

    // InputTextMultiline requires a mutable char* in ReadOnly mode (ImGui
    // API predates const-correctness for this overload).  Mutable copy
    // avoids the UB of const_cast while preserving the original string.
    std::string decompiled_copy = state.decompiler.getSourceText();

    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ui::col::v(ui::col::with_alpha(0xFF0E0F1A, 0.85f)));
    ImGui::InputTextMultiline("##decompiled",
        decompiled_copy.data(), decompiled_copy.size() + 1,
        ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}
