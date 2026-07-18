// =============================================================================
// panel_statusbar.cpp  —  Premium status bar overlay implementation.
// =============================================================================

#include "panel_statusbar.h"
#include "ui_premium.h"
#include "vm.h"

#include "imgui.h"
#include <cmath>

void RenderStatusBar(bool vm_running, bool vm_halted, const VM& vm, bool has_errors) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float bar_h = 28.0f;
    ImVec2 pos (vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h);
    ImVec2 size(vp->WorkSize.x, bar_h);

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration         |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoSavedSettings      |
        ImGuiWindowFlags_NoBringToFrontOnFocus|
        ImGuiWindowFlags_NoNavFocus           |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar  (ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding,  ImVec2(18, 8));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ui::col::v(ui::col::with_alpha(ui::col::BG_DEEP, 0.97f)));
    ImGui::Begin("##StatusBar", nullptr, kFlags);

    // ---- Animated top border: fades in from edges, peaks at centre ----------
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const int segs = 60;
        for (int i = 0; i < segs; ++i) {
            float t0 = static_cast<float>(i) / segs;
            float a  = (t0 < 0.5f) ? t0 * 2.0f : (1.0f - t0) * 2.0f;
            dl->AddLine(
                ImVec2(pos.x + size.x * t0,
                       pos.y + 0.5f),
                ImVec2(pos.x + size.x * static_cast<float>(i + 1) / segs,
                       pos.y + 0.5f),
                ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.55f * a), 1.0f);
        }
    }

    // ---- State indicator ----------------------------------------------------
    ImU32       state_col;
    const char* state_label;
    bool        pulsing = false;

    if (has_errors) {
        state_col   = ui::col::ACCENT_CORAL;
        state_label = "ERROR";
        pulsing     = true;
    } else if (vm_halted) {
        state_col   = ui::col::ACCENT_AMBER;
        state_label = "HALTED";
    } else if (vm_running) {
        state_col   = ui::col::ACCENT_GREEN;
        state_label = "RUNNING";
        pulsing     = true;
    } else {
        state_col   = ui::col::TEXT_SEC;
        state_label = "IDLE";
    }

    ui::status_dot(state_col, 5.0f, pulsing);
    ImGui::SameLine();
    ImGui::TextUnformatted(state_label);

    // ---- Spinner shown while VM is actively stepping ------------------------
    // Draws a simple rotating arc so there is a clear busy indicator.
    if (vm_running && !vm_halted && !has_errors) {
        ImGui::SameLine(0, 10);
        ImDrawList* dl     = ImGui::GetWindowDrawList();
        ImVec2      center = ImGui::GetCursorScreenPos();
        center.x += 7.0f;
        center.y += bar_h * 0.5f - 2.0f;
        const float  r     = 6.0f;
        const float  t     = static_cast<float>(ImGui::GetTime());
        const float  speed = 3.0f;
        const float  arc   = 1.4f; // radians — length of spinning arc
        dl->PathArcTo(center, r,
                      t * speed,
                      t * speed + arc, 10);
        dl->PathStroke(ui::col::with_alpha(ui::col::ACCENT_GREEN, 0.85f),
                       ImDrawFlags_None, 2.0f);
        ImGui::Dummy(ImVec2(16.0f, 0));
    }

    // ---- Register snapshot --------------------------------------------------
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::TextDisabled("PC");
    ImGui::SameLine();
    ImGui::Text("0x%02X", vm.halted ? 0xFFu : vm.pc);

    ImGui::SameLine();
    ImGui::TextDisabled("|  A");
    ImGui::SameLine();
    ImU32 a_col = (vm.accumulator == 0) ? ui::col::TEXT_SEC : ui::col::ACCENT_CYAN;
    ImGui::TextColored(ui::col::v(a_col), "0x%02X", vm.accumulator);

    ImGui::SameLine();
    ImGui::TextDisabled("|  OUT");
    ImGui::SameLine();
    ImGui::Text("0x%02X", vm.output_reg);

    // ---- Right-aligned: icon/font status ------------------------------------
    const float right_w =
        ImGui::CalcTextSize("FontAwesome: READY  |  Cosmic Aurora theme").x + 20.0f;
    const float cursor_x = ImGui::GetCursorPosX();
    const float avail_x  = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(cursor_x + avail_x - right_w);
    ImGui::TextDisabled("FontAwesome: %s  |  Cosmic Aurora theme",
                        ui::icons::has() ? "READY" : "TEXT");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
