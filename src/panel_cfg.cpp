// =============================================================================
// panel_cfg.cpp  —  Control Flow Graph visualiser panel implementation.
// =============================================================================

#include "panel_cfg.h"
#include "decompiler.h"
#include "ui_premium.h"

#include "imgui.h"
#include <cmath>
#include <vector>
#include <algorithm>

void RenderCFGWindow(const char* title, const Decompiler& decompiler, bool vm_running) {
    ImGui::Begin(title);

    const CFG& cfg = decompiler.getCFG();

    if (cfg.blocks.empty()) {
        ImGui::TextDisabled("No CFG available. Run or Load a program first.");
        ImGui::End();
        return;
    }

    // ---- Premium header strip -----------------------------------------------
    ui::header_gradient("Control Flow Graph", ui::col::ACCENT_PUR);
    ImGui::Dummy(ImVec2(0, 4));

    ui::status_dot(vm_running ? ui::col::ACCENT_GREEN : ui::col::TEXT_SEC,
                   5.0f, vm_running);
    ImGui::SameLine();
    ImGui::Text("Basic Blocks: %d   Edges: %d   %s",
                (int)cfg.blocks.size(), (int)cfg.edges.size(),
                vm_running ? "VM running \xe2\x80\x94 live data flow" : "VM idle");
    ui::separator_gradient(ui::col::ACCENT_PUR, 0.30f);

    // =========================================================================
    //  Layout pass: assign each block a layer + horizontal index.
    //  Uses BFS from block 0 to handle DAGs and cycles (back-edges ignored).
    // =========================================================================
    const int  N = static_cast<int>(cfg.blocks.size());
    std::vector<ImVec2> block_pos(N);
    std::vector<int>    block_layer(N, -1);

    {
        std::vector<bool> visited(N, false);
        std::vector<int>  q;
        block_layer[0] = 0;
        visited[0]     = true;
        q.push_back(0);
        size_t head = 0;
        while (head < q.size()) {
            int b = q[head++];
            for (int succ : cfg.blocks[b].successors) {
                if (succ >= 0 && succ < N && !visited[succ]) {
                    visited[succ]     = true;
                    block_layer[succ] = block_layer[b] + 1;
                    q.push_back(succ);
                }
            }
        }
        // Blocks unreachable from block 0 get layer 0 (best effort).
        for (int i = 0; i < N; ++i) {
            if (block_layer[i] < 0) block_layer[i] = 0;
        }
    }

    int max_layer = 0;
    for (int l : block_layer) max_layer = std::max(max_layer, l);

    std::vector<int> layer_counts(max_layer + 1, 0);
    std::vector<int> layer_index(N, 0);
    for (int i = 0; i < N; ++i) {
        layer_index[i] = layer_counts[block_layer[i]]++;
    }

    // ---- Geometry constants -------------------------------------------------
    const float block_w      = 168.0f;
    const float block_h_base = 28.0f;
    const float block_h_per  = 20.0f;
    const float layer_gap_y  = 110.0f;
    const float block_gap_x  = 36.0f;
    const float margin_x     = 24.0f;
    const float margin_y     = 24.0f;

    float canvas_w = 0.0f;
    for (int c : layer_counts) {
        float lw = static_cast<float>(c) * block_w + static_cast<float>(c - 1) * block_gap_x;
        canvas_w = std::max(canvas_w, lw);
    }
    canvas_w += margin_x * 2.0f;
    float canvas_h = margin_y * 2.0f + static_cast<float>(max_layer + 1) * layer_gap_y;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    // ---- Canvas background: dark glass card ---------------------------------
    // Shadow.
    dl->AddRectFilled(
        ImVec2(canvas_pos.x + 3, canvas_pos.y + 6),
        ImVec2(canvas_pos.x + canvas_w + 3, canvas_pos.y + canvas_h + 6),
        ui::col::with_alpha(0xFF000000, 0.40f), 8.0f);
    // Fill.
    dl->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
        ui::col::with_alpha(ui::col::BG_DEEP, 0.85f), 8.0f);
    // Top highlight.
    dl->AddLine(
        ImVec2(canvas_pos.x + 10,            canvas_pos.y + 0.5f),
        ImVec2(canvas_pos.x + canvas_w - 10, canvas_pos.y + 0.5f),
        ui::col::with_alpha(0xFFFFFFFF, 0.10f), 1.0f);
    // Border.
    dl->AddRect(canvas_pos,
        ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
        ui::col::with_alpha(ui::col::ACCENT_PUR, 0.25f), 8.0f, 1.0f, 0);

    // ---- Compute block centre positions -------------------------------------
    for (int i = 0; i < N; ++i) {
        const int layer = block_layer[i];
        const int idx   = layer_index[i];
        const int count = layer_counts[layer];

        float layer_total_w = static_cast<float>(count) * block_w
                            + static_cast<float>(count - 1) * block_gap_x;
        float layer_start_x = canvas_pos.x + margin_x
                            + (canvas_w - margin_x * 2.0f - layer_total_w) * 0.5f;
        float x = layer_start_x + static_cast<float>(idx) * (block_w + block_gap_x);
        float y = canvas_pos.y + margin_y + static_cast<float>(layer) * layer_gap_y;
        block_pos[i] = ImVec2(x + block_w * 0.5f, y);
    }

    // Helper: total height of a block (header + instruction rows).
    auto block_height = [&](const BasicBlock& bb) -> float {
        return block_h_base + static_cast<float>(bb.instructions.size()) * block_h_per;
    };

    // =========================================================================
    //  Draw edges with animated data-flow dots.
    // =========================================================================
    for (const auto& e : cfg.edges) {
        // Bounds-check both endpoint indices before dereferencing (Fix B4).
        if (e.first  < 0 || e.first  >= N) continue;
        if (e.second < 0 || e.second >= N) continue;
        if (e.first  >= (int)block_pos.size()) continue;
        if (e.second >= (int)block_pos.size()) continue;

        const ImVec2 from = block_pos[e.first];
        const ImVec2 to   = block_pos[e.second];
        ImVec2 from_bottom(from.x, from.y + block_height(cfg.blocks[e.first]));
        ImVec2 to_top(to.x, to.y);

        float  dy  = (to_top.y - from_bottom.y) * 0.5f;
        ImVec2 cp1(from_bottom.x, from_bottom.y + dy);
        ImVec2 cp2(to_top.x,     to_top.y - dy);

        // Edge colour from the last opcode of the source block.
        ImU32 edge_color = ui::col::with_alpha(ui::col::ACCENT_GREEN, 0.85f);
        const auto& src  = cfg.blocks[e.first];
        if (!src.instructions.empty()) {
            uint8_t op = src.instructions.back().opcode;
            if      (op == 0x7 || op == 0x8) edge_color = ui::col::with_alpha(ui::col::ACCENT_AMBER, 0.85f);
            else if (op == 0xF)               edge_color = ui::col::with_alpha(ui::col::ACCENT_CORAL, 0.85f);
        }

        // Soft glow + crisp edge.
        dl->AddBezierCubic(from_bottom, cp1, cp2, to_top,
            ui::col::with_alpha(edge_color, 0.20f), 5.0f);
        dl->AddBezierCubic(from_bottom, cp1, cp2, to_top, edge_color, 2.0f);

        // Arrowhead.
        const float ah = 7.0f;
        dl->AddTriangleFilled(
            to_top,
            ImVec2(to_top.x - ah, to_top.y - ah * 1.5f),
            ImVec2(to_top.x + ah, to_top.y - ah * 1.5f),
            edge_color);

        // Animated dot (only while the VM is running).
        if (vm_running) {
            double t     = ImGui::GetTime();
            float  phase = static_cast<float>(
                std::fmod(t * 0.6 + static_cast<double>(e.first * 7 + e.second * 13) * 0.1, 1.0));
            float u = 1.0f - phase;
            ImVec2 p(
                u*u*u*from_bottom.x + 3*u*u*phase*cp1.x + 3*u*phase*phase*cp2.x + phase*phase*phase*to_top.x,
                u*u*u*from_bottom.y + 3*u*u*phase*cp1.y + 3*u*phase*phase*cp2.y + phase*phase*phase*to_top.y
            );
            dl->AddCircleFilled(p, 6.0f, ui::col::with_alpha(edge_color, 0.30f), 16);
            dl->AddCircleFilled(p, 3.0f, ui::col::with_alpha(0xFFFFFFFF, 0.95f), 12);
        }
    }

    // =========================================================================
    //  Draw blocks as glass cards.
    // =========================================================================
    for (int i = 0; i < N; ++i) {
        const BasicBlock& bb     = cfg.blocks[i];
        const ImVec2      center = block_pos[i];
        const float       h      = block_height(bb);
        ImVec2 tl(center.x - block_w * 0.5f, center.y);
        ImVec2 br(center.x + block_w * 0.5f, center.y + h);

        // Drop shadow.
        dl->AddRectFilled(
            ImVec2(tl.x + 2, tl.y + 4), ImVec2(br.x + 2, br.y + 4),
            ui::col::with_alpha(0xFF000000, 0.45f), 6.0f);

        // Fill: entry block gets cyan tint.
        ImU32 fill = (i == 0)
            ? ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.18f)
            : ui::col::with_alpha(ui::col::SURFACE, 0.95f);
        dl->AddRectFilled(tl, br, fill, 6.0f);

        // Top highlight.
        dl->AddLine(
            ImVec2(tl.x + 8, tl.y + 0.5f),
            ImVec2(br.x - 8, tl.y + 0.5f),
            ui::col::with_alpha(0xFFFFFFFF, 0.12f), 1.0f);

        // Border: pulsing cyan glow for entry block, hairline for others.
        if (i == 0) {
            float p = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 2.0));
            dl->AddRect(tl, br, ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.55f + 0.30f * p),
                        6.0f, 1.8f, 0);
        } else {
            dl->AddRect(tl, br, ui::col::with_alpha(0xFFFFFFFF, 0.10f), 6.0f, 1.0f, 0);
        }

        // Header band.
        dl->AddRectFilled(
            tl, ImVec2(br.x, tl.y + 22.0f),
            ui::col::with_alpha(0xFF000000, 0.25f), 6.0f, ImDrawFlags_RoundCornersTop);

        // Block label.
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "BB%d  [0x%02X-0x%02X]%s",
                      bb.id, bb.start_addr, bb.end_addr,
                      bb.label.empty() ? "" : ("  " + bb.label).c_str());
        dl->AddText(ImVec2(tl.x + 10, tl.y + 5), ui::col::ACCENT_CYAN, hdr);

        // Instruction list.
        for (size_t k = 0; k < bb.instructions.size(); ++k) {
            const auto& di = bb.instructions[k];
            char rest[64];
            std::snprintf(rest, sizeof(rest), "%02X  %s %d",
                          di.address, di.mnemonic.c_str(), (int)di.operand);
            dl->AddText(ImVec2(tl.x + 12, tl.y + 26.0f + static_cast<float>(k) * 19.0f),
                        ui::col::TEXT_SEC, "0x");
            dl->AddText(ImVec2(tl.x + 22, tl.y + 26.0f + static_cast<float>(k) * 19.0f),
                        ui::col::TEXT_PRI, rest);
        }
    }

    ImGui::Dummy(ImVec2(canvas_w, canvas_h));
    ImGui::End();
}
