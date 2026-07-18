// =============================================================================
// panel_disasm.cpp  —  Live linear disassembler panel.
//
// Reads DisasmCache::result (pre-computed by AppState::update_disasm()) and
// renders each line with mnemonic-highlighted coloring.
// =============================================================================

#include "panel_disasm.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"

#include <sstream>
#include <string>

void RenderLiveDisassembler(const AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("Live Disassembler");

    ui::header_gradient("Linear Disassembly", ui::col::ACCENT_CYAN);
    ImGui::Dummy(ImVec2(0, 6));
    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.25f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ui::col::v(ui::col::with_alpha(0xFF0E0F1A, 0.50f)));
    ImGui::BeginChild("##disasm", ImVec2(-1, -1), true);

    std::istringstream iss(state.disasm_cache.result);
    std::string line;
    while (std::getline(iss, line)) {
        // Lines are formatted as "addr: MNEMONIC operands", e.g. "03: LDA 15"
        size_t colon = line.find(": ");
        if (colon != std::string::npos) {
            // Address prefix in dim color.
            std::string before = line.substr(0, colon + 2);
            std::string after  = line.substr(colon + 2);
            ImGui::TextDisabled("%s", before.c_str());
            ImGui::SameLine(0, 0);

            // Mnemonic in accent cyan; rest of operand in dim.
            size_t sp = after.find(' ');
            std::string mnem = (sp == std::string::npos) ? after : after.substr(0, sp);
            std::string rest = (sp == std::string::npos) ? "" : after.substr(sp);

            ImGui::TextColored(ui::col::v(ui::col::ACCENT_CYAN), "%s", mnem.c_str());
            if (!rest.empty()) {
                ImGui::SameLine(0, 0);
                ImGui::TextDisabled("%s", rest.c_str());
            }
        } else {
            ImGui::TextUnformatted(line.c_str());
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar();
}
