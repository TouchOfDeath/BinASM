// =============================================================================
// panel_vm_state.cpp  —  8-bit CPU registers, flags, and memory map panel.
// =============================================================================

#include "panel_vm_state.h"
#include "app_state.h"
#include "ui_premium.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
//  reg_card — glass card widget displaying one register name/value pair.
//  File-scope so RenderVMState doesn't need a nested lambda.
//  tooltip: shown on hover to describe the register's purpose.
// ---------------------------------------------------------------------------
static void reg_card(const char* name, const char* val, ImU32 accent,
                     const char* tooltip = nullptr) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    const float h   = 38.0f;

    // Drop shadow.
    dl->AddRectFilled(ImVec2(pos.x + 2, pos.y + 3),
                      ImVec2(pos.x + w + 2, pos.y + h + 3),
                      ui::col::with_alpha(0xFF000000, 0.30f), 6.0f);
    // Card surface.
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h),
                      ui::col::with_alpha(ui::col::SURFACE_HI, 0.6f), 6.0f);
    // Top highlight.
    dl->AddLine(ImVec2(pos.x + 8, pos.y + 0.5f),
                ImVec2(pos.x + w - 8, pos.y + 0.5f),
                ui::col::with_alpha(0xFFFFFFFF, 0.10f), 1.0f);
    // Left accent bar.
    dl->AddRectFilled(ImVec2(pos.x, pos.y + 4),
                      ImVec2(pos.x + 3, pos.y + h - 4), accent, 1.5f);
    // Border.
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h),
                ui::col::BORDER_SUB, 6.0f, 1.0f, 0);

    // Advance layout.
    ImGui::Dummy(ImVec2(w, h));

    // Tooltip on hover over the card area.
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", tooltip);
    }

    // Name label (left, secondary color).
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + 11));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_SEC));
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();

    // Value label (right, accent color).
    ImVec2 val_sz = ImGui::CalcTextSize(val);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + w - val_sz.x - 14, pos.y + 11));
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(accent));
    ImGui::TextUnformatted(val);
    ImGui::PopStyleColor();

    // Reset cursor to below the card.
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h + 4));
}

// =============================================================================
//  RenderVMState
// =============================================================================
void RenderVMState(AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));
    ImGui::Begin("VM State (8-bit CPU)");

    // ---- Registers ----------------------------------------------------------
    ui::header_gradient("Registers", ui::col::ACCENT_GREEN);
    ImGui::Dummy(ImVec2(0, 6));

    char buf[64];
    std::snprintf(buf, sizeof(buf), "0x%02X  (%d)",
                  state.vm.accumulator, state.vm.accumulator);
    reg_card("Accumulator (A)", buf, ui::col::ACCENT_CYAN,
             "Accumulator (A)\n"
             "The only general-purpose register.\n"
             "All arithmetic and logic ops read/write here.\n"
             "Instructions: LDA, LDI, ADD, SUB, OUT, STA");

    std::snprintf(buf, sizeof(buf), "%d", state.vm.pc);
    reg_card("Program Counter (PC)", buf, ui::col::ACCENT_GREEN,
             "Program Counter (PC)\n"
             "Points to the next instruction to execute.\n"
             "Increments by 1 after each instruction.\n"
             "JMP, JC, JZ can redirect it to any address 0-15.");

    std::snprintf(buf, sizeof(buf), "0x%02X  ('%c')", state.vm.output_reg,
                  state.vm.output_reg >= 32 ? state.vm.output_reg : '.');
    reg_card("Output (OUT)", buf, ui::col::ACCENT_MAG,
             "Output Register (OUT)\n"
             "Latches the Accumulator value when OUT executes.\n"
             "Printed to the Console Output panel.");

    // ---- CPU Flags ----------------------------------------------------------
    ImGui::Dummy(ImVec2(0, 6));
    ui::header_gradient("CPU Flags", ui::col::ACCENT_AMBER);
    ImGui::Dummy(ImVec2(0, 6));

    {
        ImU32 col = state.vm.carry_flag ? ui::col::ACCENT_GREEN : ui::col::TEXT_SEC;
        ui::status_dot(col, 5.0f, state.vm.carry_flag);
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(col), "Carry  (C)  %s",
                           state.vm.carry_flag ? "SET" : "---");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Carry Flag (C)\n"
                              "Set by ADD when the result exceeds 255 (overflow).\n"
                              "Set by SUB when a borrow occurs (underflow).\n"
                              "Read by JC — jumps if carry is SET.");
    }
    ImGui::Dummy(ImVec2(0, 4));
    {
        ImU32 col = state.vm.zero_flag ? ui::col::ACCENT_CYAN : ui::col::TEXT_SEC;
        ui::status_dot(col, 5.0f, state.vm.zero_flag);
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(col), "Zero   (Z)  %s",
                           state.vm.zero_flag ? "SET" : "---");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Zero Flag (Z)\n"
                              "Set when the Accumulator becomes 0 after LDA, LDI, ADD, or SUB.\n"
                              "Read by JZ — jumps if zero is SET.");
    }

    // ---- Memory Map ---------------------------------------------------------
    ui::separator_gradient(ui::col::ACCENT_GREEN, 0.25f);
    ImGui::Dummy(ImVec2(0, 4));
    ui::header_gradient("Memory Map (16 bytes)", ui::col::ACCENT_PUR);
    ImGui::Dummy(ImVec2(0, 4));

    for (int i = 0; i < 16; ++i) {
        bool is_pc = (i == state.vm.pc && !state.vm.halted);

        // Binary string representation.
        std::string bin;
        bin.reserve(8);
        for (int b = 7; b >= 0; --b)
            bin += ((state.vm.memory[i] >> b) & 1) ? '1' : '0';

        ImGui::TextDisabled("%02d", i);
        ImGui::SameLine(0, 8);

        char pill[8];
        std::snprintf(pill, sizeof(pill), "0x%02X", state.vm.memory[i]);

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(
                is_pc ? ui::col::ACCENT_GREEN : ui::col::SURFACE_HI,
                is_pc ? 0.30f : 0.45f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.22f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.40f)));
        ImGui::Selectable(pill, false, ImGuiSelectableFlags_None, ImVec2(60, 0));
        // Tooltip: decode the byte as opcode + operand for quick inspection.
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            uint8_t byte    = state.vm.memory[i];
            uint8_t opcode  = byte >> 4;
            uint8_t operand = byte & 0x0F;
            ImGui::SetTooltip(
                "Addr %2d  |  0x%02X  |  %s\n"
                "  Opcode : 0x%X (%s)\n"
                "  Operand: %d (0x%X)%s",
                i, byte, bin.c_str(),
                opcode,
                (opcode == 0x0 ? "NOP"  : opcode == 0x1 ? "LDA"  :
                 opcode == 0x2 ? "ADD"  : opcode == 0x3 ? "SUB"  :
                 opcode == 0x4 ? "STA"  : opcode == 0x5 ? "LDI"  :
                 opcode == 0x6 ? "JMP"  : opcode == 0x7 ? "JC"   :
                 opcode == 0x8 ? "JZ"   : opcode == 0xE ? "OUT"  :
                 opcode == 0xF ? "HLT"  : "???"),
                operand, operand,
                is_pc ? "  <- next instruction" : "");
        }
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        ImGui::SameLine(0, 8);
        if (is_pc) {
            ui::status_dot(ui::col::ACCENT_GREEN, 4.0f, true);
            ImGui::SameLine(0, 6);
            ImGui::TextColored(ui::col::v(ui::col::ACCENT_GREEN),
                               "%s  <- PC", bin.c_str());
        } else {
            ImGui::TextDisabled("%s", bin.c_str());
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
