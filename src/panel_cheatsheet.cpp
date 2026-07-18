// =============================================================================
// panel_cheatsheet.cpp  —  ISA reference / opcode cheat sheet panel.
//
// Uses isa.h as the single opcode table — no local copy.
// =============================================================================

#include "panel_cheatsheet.h"
#include "isa.h"
#include "ui_premium.h"

#include "imgui.h"

void RenderCheatSheetPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("Opcode Cheat Sheet");

    ui::header_gradient("ISA Reference", ui::col::ACCENT_CYAN);
    ImGui::Dummy(ImVec2(0, 6));
    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.25f);

    if (ImGui::BeginTable("cheat_sheet", 3,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Mnemonic");
        ImGui::TableSetupColumn("Opcode (4-bit)");
        ImGui::TableSetupColumn("Description");
        ImGui::TableHeadersRow();

        for (int i = 0; i < ISA_COUNT; ++i) {
            const IsaEntry& e = ISA_TABLE[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_CYAN), "%s", e.mnemonic);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_MAG),  "%s", e.bits);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", e.description);
        }
        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
