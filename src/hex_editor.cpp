// =============================================================================
// hex_editor.cpp - Implementation of the HexEditor ImGui widget.
// =============================================================================
//
//  See hex_editor.h for the architectural overview.
//
//  Key implementation notes:
//
//  1. Selection model.
//     We keep a single `selected_addr_` (-1 = nothing selected). When the
//     user clicks a hex cell, we set `selected_addr_` to that byte and
//     reset the edit buffer (`edit_nibble_count_ = 0`). When the user
//     clicks elsewhere, selection is cleared.
//
//  2. Edit model.
//     Once a cell is selected, the widget captures keyboard input via
//     `ImGui::GetIO().InputQueueCharacters`. Each typed hex digit is
//     shifted into `edit_buffer_`. After two digits, the byte is
//     committed, `last_edited_addr_` is set, and selection advances to
//     the next cell (wrapping within the memory range).
//
//  3. Why not use ImGui::InputText per cell?
//     InputText would require per-cell state buffers and would fight us
//     on focus management (clicking a different cell would need to defocus
//     the previous InputText first). A custom Selectable + keystroke
//     handler gives us the snappy HxD-style UX with much less code.
//
//  4. Visual feedback.
//     The selected cell is drawn with the ImGui HeaderActive color.
//     The PC cell is drawn with a green tint.
//     A byte edited this frame briefly flashes a yellow tint (one frame
//     is enough because the user will see the new value persist).
//
// =============================================================================

#include "hex_editor.h"
#include "imgui.h"
#include "ui_premium.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>

// -----------------------------------------------------------------------------
//  Constructor - sensible defaults.
// -----------------------------------------------------------------------------
HexEditor::HexEditor()
    : selected_addr_(-1),
      last_edited_addr_(-1),
      edit_nibble_count_(0),
      edit_buffer_(0),
      bytes_per_row_(8),
      show_ascii_(true),
      read_only_(false) {}

// -----------------------------------------------------------------------------
//  Hex digit helpers.
// -----------------------------------------------------------------------------
bool HexEditor::isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}
int HexEditor::hexDigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

// -----------------------------------------------------------------------------
//  Top-level render function.
//
//  We open an ImGui window with the given title, render a header row,
//  then iterate the memory in chunks of `bytes_per_row_` and render each
//  row. Keyboard input is handled once per frame (not per cell).
// -----------------------------------------------------------------------------
bool HexEditor::render(const char* title, uint8_t* memory, int size,
                       uint8_t pc_highlight) {
    // Reset the per-frame "edited" flag. It will be set true by
    // handleKeyboard() or by a direct click-edit if we ever add one.
    last_edited_addr_ = -1;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::Begin(title);

    // ---- Premium header ----
    ui::header_gradient("Memory Inspector", ui::col::ACCENT_MAG);
    ImGui::Dummy(ImVec2(0, 4));

    // ---- Toolbar (styled) ----
    // Editable / Read-Only toggle as a small pill button.
    ImGui::PushStyleColor(ImGuiCol_Button,
        ui::col::v(ui::col::with_alpha(
            read_only_ ? ui::col::ACCENT_AMBER : ui::col::ACCENT_GREEN, 0.20f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ui::col::v(ui::col::with_alpha(
            read_only_ ? ui::col::ACCENT_AMBER : ui::col::ACCENT_GREEN, 0.40f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ui::col::v(ui::col::with_alpha(
            read_only_ ? ui::col::ACCENT_AMBER : ui::col::ACCENT_GREEN, 0.55f)));
    if (ImGui::Button(read_only_ ? "Read-Only" : "Editable")) {
        read_only_ = !read_only_;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("Bytes/Row:");
    ImGui::SameLine();
    ImGui::PushItemWidth(60);
    int bpr = bytes_per_row_;
    if (ImGui::InputInt("##bpr", &bpr, 1, 1)) {
        setBytesPerRow(bpr);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Checkbox("ASCII", &show_ascii_);
    ImGui::SameLine();
    ImGui::TextDisabled("| Click a byte, type 2 hex digits (e.g. '1F')");

    ui::separator_gradient(ui::col::ACCENT_MAG, 0.25f);

    // ---- Header row ----
    // Reserve space for the address column, then label each hex column
    // with its offset (0..bytes_per_row_-1).
    ImGui::Dummy(ImVec2(kAddrWidth, 0));
    ImGui::SameLine();
    for (int i = 0; i < bytes_per_row_; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X", i);
        ImGui::TextDisabled("%s", buf);
        ImGui::SameLine(0.0f, kCellWidth - ImGui::CalcTextSize(buf).x);
    }
    if (show_ascii_) {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();
        for (int i = 0; i < bytes_per_row_; ++i) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%X", i);
            ImGui::TextDisabled("%s", buf);
            ImGui::SameLine(0.0f, kAsciiCellWidth - ImGui::CalcTextSize(buf).x);
        }
    }
    ui::separator_gradient(ui::col::ACCENT_MAG, 0.15f);

    // ---- Data rows ----
    for (int row_start = 0; row_start < size; row_start += bytes_per_row_) {
        renderRow(memory, size, row_start, pc_highlight);
    }

    // ---- Keyboard input (once per frame) ----
    bool edited = (last_edited_addr_ == -1) && (selected_addr_ >= 0)
                  ? handleKeyboard(memory, size)
                  : (last_edited_addr_ != -1);

    // ---- Footer (premium status row) ----
    ui::separator_gradient(ui::col::ACCENT_MAG, 0.20f);
    if (selected_addr_ >= 0 && selected_addr_ < size) {
        uint8_t b = memory[selected_addr_];
        char bin[9];
        for (int i = 7; i >= 0; --i) {
            bin[7 - i] = ((b >> i) & 1) ? '1' : '0';
        }
        bin[8] = '\0';

        ui::status_dot(ui::col::ACCENT_CYAN, 4.0f, false);
        ImGui::SameLine();
        ImGui::TextDisabled("Addr");
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_CYAN), "0x%02X", selected_addr_);
        ImGui::SameLine();
        ImGui::TextDisabled("| Hex");
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_MAG), "%02X", b);
        ImGui::SameLine();
        ImGui::TextDisabled("| Dec");
        ImGui::SameLine();
        ImGui::Text("%3d", b);
        ImGui::SameLine();
        ImGui::TextDisabled("| Bin");
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_GREEN), "%s", bin);
        ImGui::SameLine();
        ImGui::TextDisabled("| ASCII");
        ImGui::SameLine();
        ImGui::Text("'%c'", (b >= 32 && b < 127) ? (char)b : '.');
    } else {
        ImGui::TextDisabled("No byte selected. Click any hex cell to edit.");
    }

    ImGui::End();
    ImGui::PopStyleVar();
    return edited || last_edited_addr_ != -1;
}

// -----------------------------------------------------------------------------
//  Render one row of the hex grid.
// -----------------------------------------------------------------------------
void HexEditor::renderRow(uint8_t* memory, int size, int row_start_addr,
                          uint8_t pc_highlight) {
    // Address label.
    char addr_buf[16];
    std::snprintf(addr_buf, sizeof(addr_buf), "0x%02X", row_start_addr);
    ImGui::TextUnformatted(addr_buf);
    ImGui::SameLine(0.0f, kAddrWidth - ImGui::CalcTextSize(addr_buf).x);

    // Hex cells.
    for (int i = 0; i < bytes_per_row_; ++i) {
        int addr = row_start_addr + i;
        if (addr >= size) {
            // Out-of-range cell: render a blank.
            ImGui::Dummy(ImVec2(kCellWidth, kCellHeight));
            ImGui::SameLine();
            continue;
        }
        renderHexCell(memory, addr, pc_highlight);
        ImGui::SameLine();
    }

    // Optional ASCII column.
    if (show_ascii_) {
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();
        for (int i = 0; i < bytes_per_row_; ++i) {
            int addr = row_start_addr + i;
            if (addr >= size) {
                ImGui::Dummy(ImVec2(kAsciiCellWidth, kCellHeight));
                ImGui::SameLine();
                continue;
            }
            renderAsciiCell(memory, addr, pc_highlight);
            ImGui::SameLine();
        }
    }

    ImGui::NewLine();
}

// -----------------------------------------------------------------------------
//  Render a single hex cell.
//
//  Visual states (mutually exclusive, highest priority first):
//    1. Edited this frame    -> yellow background
//    2. PC address           -> green background
//    3. Selected             -> blue background (HeaderActive color)
//    4. Hovered              -> subtle highlight (handled by ImGui)
//
//  The cell label shows:
//    - The two hex digits of the byte, OR
//    - A single hex digit + a cursor '_' while the user is typing the
//      second digit.
// -----------------------------------------------------------------------------
bool HexEditor::renderHexCell(uint8_t* memory, int addr, uint8_t pc_highlight) {
    ImGui::PushID(addr);

    char label[8];
    bool is_selected   = (selected_addr_ == addr);
    bool is_pc         = (pc_highlight != 0xFF && (int)pc_highlight == addr);
    bool just_edited   = (last_edited_addr_ == addr);

    // Build the cell label.
    if (is_selected && edit_nibble_count_ == 1 && !read_only_) {
        // Mid-edit: show the first typed digit and an underscore cursor.
        std::snprintf(label, sizeof(label), "%X_", (edit_buffer_ >> 4) & 0xF);
    } else {
        std::snprintf(label, sizeof(label), "%02X", memory[addr]);
    }

    // Choose background color using the premium palette.
    // The PC cell pulses subtly so it always draws the eye.
    float pc_pulse = 0.5f + 0.5f * (float)std::sin(ImGui::GetTime() * 3.0);
    if (just_edited) {
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_AMBER, 0.55f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_AMBER, 0.70f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_AMBER, 0.85f)));
    } else if (is_pc) {
        // PC cell: green with a subtle pulsing brightness.
        float a = 0.45f + 0.25f * pc_pulse;
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a + 0.15f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a + 0.30f)));
    } else if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.30f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.45f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.60f)));
    }

    // Render the cell as a Selectable. The boolean tracks selection state.
    bool clicked = ImGui::Selectable(label, is_selected,
                                     ImGuiSelectableFlags_None,
                                     ImVec2(kCellWidth, kCellHeight));

    if (just_edited || is_pc || is_selected) {
        ImGui::PopStyleColor(3);
    }

    // Optional accent border on the PC cell to make it pop even more.
    if (is_pc) {
        ImVec2 cell_pos = ImGui::GetItemRectMin();
        ImVec2 cell_max = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float a = 0.6f + 0.4f * pc_pulse;
        dl->AddRect(cell_pos, cell_max,
                    ui::col::with_alpha(ui::col::ACCENT_GREEN, a),
                    3.0f, 1.5f, 0);
    }

    if (clicked && !read_only_) {
        // If the user clicked the SAME cell that was already selected and
        // mid-edit, do nothing (let them keep typing). Otherwise, switch
        // selection to this cell and reset the edit buffer.
        if (selected_addr_ != addr) {
            selected_addr_ = addr;
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
        }
    }

    ImGui::PopID();
    return clicked;
}

// -----------------------------------------------------------------------------
//  Render a single ASCII cell (display only).
// -----------------------------------------------------------------------------
void HexEditor::renderAsciiCell(const uint8_t* memory, int addr,
                                uint8_t pc_highlight) {
    ImGui::PushID(0x10000 | addr);

    uint8_t b = memory[addr];
    char ch = (b >= 32 && b < 127) ? (char)b : '.';
    char label[2] = { ch, '\0' };

    bool is_pc = (pc_highlight != 0xFF && (int)pc_highlight == addr);
    bool is_selected = (selected_addr_ == addr);

    if (is_pc) {
        float p = 0.5f + 0.5f * (float)std::sin(ImGui::GetTime() * 3.0);
        float a = 0.45f + 0.25f * p;
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a + 0.15f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_GREEN, a + 0.30f)));
    } else if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Header,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.30f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.45f)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
            ui::col::v(ui::col::with_alpha(ui::col::ACCENT_CYAN, 0.60f)));
    }

    // Clicking the ASCII cell also selects the byte for editing.
    if (ImGui::Selectable(label, is_selected,
                          ImGuiSelectableFlags_None,
                          ImVec2(kAsciiCellWidth, kCellHeight))) {
        if (!read_only_ && selected_addr_ != addr) {
            selected_addr_ = addr;
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
        }
    }

    if (is_pc || is_selected) ImGui::PopStyleColor(3);
    ImGui::PopID();
}

// -----------------------------------------------------------------------------
//  Keyboard input handler.
//
//  Active only when a cell is selected and we are not in read-only mode.
//  Reads characters from ImGui's InputQueueCharacters (which already
//  filters out modifiers and key repeats we do not want).
//
//  Keys handled:
//    [0-9 a-f A-F]  -> accumulate as the next hex digit.
//                     After 2 digits: commit byte, advance selection.
//    Backspace      -> if mid-edit, cancel the pending digit;
//                     otherwise move selection to the previous cell.
//    Left/Right     -> navigate selection (cancels any pending edit).
//    Up/Down        -> navigate selection by +/- bytes_per_row.
//    Enter          -> commit the pending digit (if any) as the high nibble,
//                     padded with 0; advance to next cell.
//    Escape         -> cancel edit and clear selection.
//
//  Returns true if a byte was edited (committed) during this call.
// -----------------------------------------------------------------------------
bool HexEditor::handleKeyboard(uint8_t* memory, int size) {
    if (read_only_ || selected_addr_ < 0 || selected_addr_ >= size) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool edited = false;

    // 1. Hex digit input.
    for (int n = 0; n < io.InputQueueCharacters.Size; ++n) {
        char c = static_cast<char>(io.InputQueueCharacters[n]);
        if (isHexDigit(c)) {
            int v = hexDigitValue(c);
            edit_buffer_ = static_cast<uint8_t>((edit_buffer_ << 4) | v);
            ++edit_nibble_count_;
            // Consume this character so it doesn't propagate.
            io.InputQueueCharacters[n] = 0;

            if (edit_nibble_count_ == 2) {
                // Commit the byte.
                memory[selected_addr_] = edit_buffer_;
                last_edited_addr_ = selected_addr_;
                edited = true;

                // Reset edit state and advance selection.
                edit_nibble_count_ = 0;
                edit_buffer_ = 0;
                if (selected_addr_ + 1 < size) {
                    ++selected_addr_;
                } else {
                    // Wrap to the start (matches HxD behaviour at the end
                    // of the address space).
                    selected_addr_ = 0;
                }
            }
        }
    }

    // 2. Backspace.
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (edit_nibble_count_ > 0) {
            // Cancel the pending digit.
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
        } else if (selected_addr_ > 0) {
            --selected_addr_;
        }
    }

    // 3. Arrow keys.
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        edit_nibble_count_ = 0;
        edit_buffer_ = 0;
        if (selected_addr_ > 0) --selected_addr_;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        edit_nibble_count_ = 0;
        edit_buffer_ = 0;
        if (selected_addr_ + 1 < size) ++selected_addr_;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        edit_nibble_count_ = 0;
        edit_buffer_ = 0;
        if (selected_addr_ - bytes_per_row_ >= 0) {
            selected_addr_ -= bytes_per_row_;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        edit_nibble_count_ = 0;
        edit_buffer_ = 0;
        if (selected_addr_ + bytes_per_row_ < size) {
            selected_addr_ += bytes_per_row_;
        }
    }

    // 4. Enter - commit a single typed digit as the high nibble (low nibble = 0).
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (edit_nibble_count_ == 1) {
            memory[selected_addr_] = static_cast<uint8_t>(edit_buffer_ << 4);
            last_edited_addr_ = selected_addr_;
            edited = true;
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
            if (selected_addr_ + 1 < size) ++selected_addr_;
        }
    }

    // 5. Escape - cancel.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        edit_nibble_count_ = 0;
        edit_buffer_ = 0;
        selected_addr_ = -1;
    }

    return edited;
}
