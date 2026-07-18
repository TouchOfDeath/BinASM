// =============================================================================
// memory_grid.cpp - Interactive Memory Grid Viewer implementation.
// =============================================================================

#include "memory_grid.h"
#include "vm.h"
#include "isa.h"
#include "ui_premium.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstring>

// =============================================================================
//  MemoryGrid Implementation
// =============================================================================

MemoryGrid::MemoryGrid()
    : selected_addr_(-1)
    , hovered_addr_(-1)
    , last_edited_addr_(-1)
    , edit_nibble_count_(0)
    , edit_buffer_(0)
    , view_mode_(MemoryViewMode::Hex)
    , color_strategy_(ColorStrategy::ByValueRange)
    , show_binary_bars_(true)
    , show_ascii_(true)
    , columns_(8) {
}

ImVec4 MemoryGrid::get_value_color(uint8_t value, int addr, bool is_pc) const {
    if (is_pc) {
        return ui::col::v(ui::col::ACCENT_GREEN);
    }
    
    switch (color_strategy_) {
        case ColorStrategy::None:
            return ui::col::v(ui::col::TEXT_PRI);
            
        case ColorStrategy::ByValueRange:
            // Color based on numeric value ranges
            if (value == 0) {
                return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray for zero
            } else if (value < 16) {
                return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);  // Light blue for small
            } else if (value < 64) {
                return ImVec4(0.3f, 1.0f, 0.5f, 1.0f);  // Green for medium
            } else if (value < 128) {
                return ImVec4(1.0f, 0.9f, 0.3f, 1.0f);  // Yellow for larger
            } else if (value < 200) {
                return ImVec4(1.0f, 0.6f, 0.3f, 1.0f);  // Orange for high
            } else {
                return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red for very high
            }
            
        case ColorStrategy::ByInstruction: {
            // Color based on opcode category (upper nibble)
            uint8_t opcode = value >> 4;
            switch (opcode) {
                case 0x0:  // NOP
                    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                case 0x1:  // LDA - Data Transfer
                case 0x4:  // STA
                case 0x5:  // LDI
                    return ImVec4(0.3f, 0.7f, 1.0f, 1.0f);  // Blue
                case 0x2:  // ADD - Arithmetic
                case 0x3:  // SUB
                    return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);  // Red
                case 0x6:  // JMP - Control Flow
                case 0x7:  // JC
                case 0x8:  // JZ
                    return ImVec4(1.0f, 0.7f, 0.3f, 1.0f);  // Orange
                case 0xE:  // OUT - I/O
                    return ImVec4(0.7f, 0.5f, 1.0f, 1.0f);  // Purple
                case 0xF:  // HLT - Control
                    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
                default:
                    return ui::col::v(ui::col::TEXT_PRI);
            }
        }
        
        case ColorStrategy::ByUsage:
            // Simple code vs data distinction
            // Treat values with valid opcodes as code
            {
                uint8_t opcode = value >> 4;
                bool is_code = (opcode <= 0x8 || opcode == 0xE || opcode == 0xF);
                return is_code 
                    ? ImVec4(0.3f, 0.7f, 1.0f, 1.0f)  // Blue for code
                    : ImVec4(0.3f, 1.0f, 0.5f, 1.0f);  // Green for data
            }
    }
    
    return ui::col::v(ui::col::TEXT_PRI);
}

void MemoryGrid::draw_binary_bar(ImVec2 pos, uint8_t value, float width, float height) const {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    
    float bit_width = width / 8.0f;
    
    for (int i = 7; i >= 0; --i) {
        bool bit_set = (value >> i) & 1;
        float x = pos.x + (7 - i) * bit_width;
        
        if (bit_set) {
            // Draw filled segment for set bits
            dl->AddRectFilled(
                ImVec2(x + 1, pos.y + 2),
                ImVec2(x + bit_width - 1, pos.y + height - 2),
                ui::col::with_alpha(ui::col::ACCENT_GREEN, 0.6f),
                1.0f
            );
        } else {
            // Draw outline for clear bits
            dl->AddRect(
                ImVec2(x + 1, pos.y + 2),
                ImVec2(x + bit_width - 1, pos.y + height - 2),
                ui::col::with_alpha(ui::col::TEXT_SEC, 0.3f),
                1.0f
            );
        }
    }
}

bool MemoryGrid::render_cell(VM& vm, int addr, bool is_pc, float cell_width) {
    ImGui::PushID(addr);
    
    uint8_t value = vm.memory[addr];
    bool edited = false;
    bool is_selected = (selected_addr_ == addr);
    bool just_edited = (last_edited_addr_ == addr);
    
    // Build label based on view mode
    char label[16];
    switch (view_mode_) {
        case MemoryViewMode::Hex:
            std::snprintf(label, sizeof(label), "%02X", value);
            break;
        case MemoryViewMode::Decimal:
            std::snprintf(label, sizeof(label), "%3d", value);
            break;
        case MemoryViewMode::Binary:
            label[0] = '\0';  // No text for binary mode
            break;
        case MemoryViewMode::Mixed:
            if (value >= 32 && value < 127) {
                std::snprintf(label, sizeof(label), "%c", value);
            } else {
                std::snprintf(label, sizeof(label), "%02X", value);
            }
            break;
    }
    
    // Get color for this value
    ImVec4 color = get_value_color(value, addr, is_pc);
    
    // Handle mid-edit state
    if (is_selected && edit_nibble_count_ == 1 && view_mode_ == MemoryViewMode::Hex) {
        std::snprintf(label, sizeof(label), "%X_", (edit_buffer_ >> 4) & 0xF);
    }
    
    // Push style colors
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.7f));
    
    if (just_edited) {
        // Flash effect for recently edited cells
        ImGui::PushStyleColor(ImGuiCol_Button, ui::col::v(ui::col::ACCENT_AMBER));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui::col::v(ui::col::ACCENT_AMBER));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ui::col::v(ui::col::ACCENT_AMBER));
    }
    
    if (is_pc) {
        // PC highlight pulse effect
        float pulse = 0.5f + 0.5f * (float)std::sin(ImGui::GetTime() * 3.0);
        ImVec4 pc_color = ImVec4(0.3f, 0.9f, 0.3f, 0.4f + 0.3f * pulse);
        ImGui::PushStyleColor(ImGuiCol_Button, pc_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.9f, 0.3f, 0.8f));
    }
    
    // Render the cell
    bool clicked = false;
    if (view_mode_ == MemoryViewMode::Binary) {
        // Binary visualization mode
        ImGui::InvisibleButton("##binary", ImVec2(cell_width, 24.0f));
        if (ImGui::IsItemHovered()) {
            hovered_addr_ = addr;
            clicked = ImGui::IsMouseClicked(0);
        }
        if (ImGui::IsItemClicked()) {
            selected_addr_ = addr;
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
        }
        // Draw binary bar
        ImVec2 pos = ImGui::GetItemRectMin();
        draw_binary_bar(pos, value, cell_width, 24.0f);
    } else {
        // Text-based modes
        clicked = ImGui::Button(label, ImVec2(cell_width, 24.0f));
        if (ImGui::IsItemHovered()) {
            hovered_addr_ = addr;
        }
    }
    
    // Pop style colors
    int pop_count = is_pc ? 6 : (just_edited ? 6 : 3);
    ImGui::PopStyleColor(pop_count);
    
    // Handle click
    if (clicked) {
        if (selected_addr_ != addr) {
            selected_addr_ = addr;
            edit_nibble_count_ = 0;
            edit_buffer_ = 0;
        }
    }
    
    // Tooltip
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        char tooltip[128];
        const char* mnemonic = isa_mnemonic(value >> 4);
        std::snprintf(tooltip, sizeof(tooltip),
            "Address: %d (0x%02X)\n"
            "Value: 0x%02X (%d)\n"
            "Binary: %08b\n"
            "%s%s",
            addr, addr,
            value, value,
            value,
            is_pc ? "← Program Counter\n" : "",
            (mnemonic && strcmp(mnemonic, "???") != 0) ? 
                mnemonic : "");
        ImGui::SetTooltip("%s", tooltip);
    }
    
    ImGui::PopID();
    return edited;
}

bool MemoryGrid::render(const char* title, VM& vm) {
    last_edited_addr_ = -1;
    hovered_addr_ = -1;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::Begin(title);
    
    // Header
    ui::header_gradient("Memory Grid Inspector", ui::col::ACCENT_PUR);
    ImGui::Dummy(ImVec2(0, 4));
    
    // Toolbar
    ImGui::TextDisabled("View Mode:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Hex")) { view_mode_ = MemoryViewMode::Hex; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Decimal")) { view_mode_ = MemoryViewMode::Decimal; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Binary")) { view_mode_ = MemoryViewMode::Binary; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Mixed")) { view_mode_ = MemoryViewMode::Mixed; }
    
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("Color:");
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) { color_strategy_ = ColorStrategy::None; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Range")) { color_strategy_ = ColorStrategy::ByValueRange; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Opcode")) { color_strategy_ = ColorStrategy::ByInstruction; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Usage")) { color_strategy_ = ColorStrategy::ByUsage; }
    
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Binary Bars", &show_binary_bars_);
    ImGui::SameLine();
    ImGui::Checkbox("ASCII", &show_ascii_);
    
    ui::separator_gradient(ui::col::ACCENT_PUR, 0.25f);
    ImGui::Dummy(ImVec2(0, 4));
    
    // Calculate cell width
    float avail_width = ImGui::GetContentRegionAvail().x;
    float cell_width = (avail_width - 10.0f) / columns_;
    if (cell_width < 20.0f) cell_width = 20.0f;
    
    // Render grid
    bool memory_modified = false;
    for (int row = 0; row < 16; row += columns_) {
        for (int col = 0; col < columns_ && (row + col) < 16; ++col) {
            int addr = row + col;
            bool is_pc = (addr == vm.pc && !vm.halted);
            
            if (col > 0) ImGui::SameLine();
            
            ImGui::PushID(addr);
            
            uint8_t value = vm.memory[addr];
            bool is_selected = (selected_addr_ == addr);
            bool just_edited = (last_edited_addr_ == addr);
            
            // Build label based on view mode
            char label[16];
            switch (view_mode_) {
                case MemoryViewMode::Hex:
                    std::snprintf(label, sizeof(label), "%02X", value);
                    break;
                case MemoryViewMode::Decimal:
                    std::snprintf(label, sizeof(label), "%3d", value);
                    break;
                case MemoryViewMode::Binary:
                    label[0] = '\0';
                    break;
                case MemoryViewMode::Mixed:
                    if (value >= 32 && value < 127) {
                        std::snprintf(label, sizeof(label), "%c", value);
                    } else {
                        std::snprintf(label, sizeof(label), "%02X", value);
                    }
                    break;
            }
            
            // Get color for this value
            ImVec4 color = get_value_color(value, addr, is_pc);
            
            // Handle mid-edit state
            if (is_selected && edit_nibble_count_ == 1 && view_mode_ == MemoryViewMode::Hex) {
                std::snprintf(label, sizeof(label), "%X_", (edit_buffer_ >> 4) & 0xF);
            }
            
            // Push style colors
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.7f));
            
            if (just_edited) {
                ImGui::PushStyleColor(ImGuiCol_Button, ui::col::v(ui::col::ACCENT_AMBER));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ui::col::v(ui::col::ACCENT_AMBER));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ui::col::v(ui::col::ACCENT_AMBER));
            }
            
            if (is_pc) {
                float pulse = 0.5f + 0.5f * (float)std::sin(ImGui::GetTime() * 3.0);
                ImVec4 pc_color = ImVec4(0.3f, 0.9f, 0.3f, 0.4f + 0.3f * pulse);
                ImGui::PushStyleColor(ImGuiCol_Button, pc_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.9f, 0.3f, 0.8f));
            }
            
            // Render the cell
            bool clicked = false;
            if (view_mode_ == MemoryViewMode::Binary) {
                ImGui::InvisibleButton("##binary", ImVec2(cell_width, 24.0f));
                if (ImGui::IsItemHovered()) {
                    hovered_addr_ = addr;
                    clicked = ImGui::IsMouseClicked(0);
                }
                if (ImGui::IsItemClicked()) {
                    selected_addr_ = addr;
                    edit_nibble_count_ = 0;
                    edit_buffer_ = 0;
                }
                // Draw binary bar
                ImVec2 pos = ImGui::GetItemRectMin();
                draw_binary_bar(pos, value, cell_width, 24.0f);
            } else {
                clicked = ImGui::Button(label, ImVec2(cell_width, 24.0f));
                if (ImGui::IsItemHovered()) {
                    hovered_addr_ = addr;
                }
            }
            
            // Pop style colors
            int pop_count = is_pc ? 6 : (just_edited ? 6 : 3);
            ImGui::PopStyleColor(pop_count);
            
            // Handle click
            if (clicked) {
                if (selected_addr_ != addr) {
                    selected_addr_ = addr;
                    edit_nibble_count_ = 0;
                    edit_buffer_ = 0;
                }
            }
            
            // Handle keyboard input for editing
            if (is_selected && view_mode_ == MemoryViewMode::Hex) {
                ImGuiIO& io = ImGui::GetIO();
                for (int n = 0; n < io.InputQueueCharacters.Size; ++n) {
                    char c = static_cast<char>(io.InputQueueCharacters[n]);
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                        int v = (c >= '0' && c <= '9') ? (c - '0') :
                                (c >= 'a' && c <= 'f') ? (10 + c - 'a') : (10 + c - 'A');
                        edit_buffer_ = static_cast<uint8_t>((edit_buffer_ << 4) | v);
                        ++edit_nibble_count_;
                        io.InputQueueCharacters[n] = 0;
                        
                        if (edit_nibble_count_ == 2) {
                            vm.memory[addr] = edit_buffer_;
                            last_edited_addr_ = addr;
                            memory_modified = true;
                            edit_nibble_count_ = 0;
                            edit_buffer_ = 0;
                            if (addr + 1 < 16) {
                                selected_addr_ = addr + 1;
                            } else {
                                selected_addr_ = 0;
                            }
                        }
                    }
                }
                
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    selected_addr_ = -1;
                    edit_nibble_count_ = 0;
                    edit_buffer_ = 0;
                }
                
                if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                    if (edit_nibble_count_ > 0) {
                        edit_nibble_count_ = 0;
                        edit_buffer_ = 0;
                    } else if (addr > 0) {
                        selected_addr_ = addr - 1;
                    }
                }
                
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && addr + 1 < 16) {
                    selected_addr_ = addr + 1;
                    edit_nibble_count_ = 0;
                    edit_buffer_ = 0;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && addr > 0) {
                    selected_addr_ = addr - 1;
                    edit_nibble_count_ = 0;
                    edit_buffer_ = 0;
                }
            }
            
            // Tooltip
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                char tooltip[256];
                const char* mnemonic = isa_mnemonic(value >> 4);
                std::string bin;
                for (int b = 7; b >= 0; --b) {
                    bin += ((value >> b) & 1) ? '1' : '0';
                }
                std::snprintf(tooltip, sizeof(tooltip),
                    "Address: %d (0x%02X)\n"
                    "Value: 0x%02X (%d)\n"
                    "Binary: %s\n"
                    "%s%s",
                    addr, addr,
                    value, value,
                    bin.c_str(),
                    is_pc ? "← Program Counter\n" : "",
                    (mnemonic && strcmp(mnemonic, "???") != 0) ? 
                        mnemonic : "");
                ImGui::SetTooltip("%s", tooltip);
            }
            
            ImGui::PopID();
        }
        ImGui::NewLine();
    }
    
    // Status bar
    ui::separator_gradient(ui::col::ACCENT_PUR, 0.20f);
    if (selected_addr_ >= 0 && selected_addr_ < 16) {
        uint8_t b = vm.memory[selected_addr_];
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
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_PUR), "%02X", b);
        ImGui::SameLine();
        ImGui::TextDisabled("| Dec");
        ImGui::SameLine();
        ImGui::Text("%3d", b);
        ImGui::SameLine();
        ImGui::TextDisabled("| Bin");
        ImGui::SameLine();
        ImGui::TextColored(ui::col::v(ui::col::ACCENT_GREEN), "%s", bin);
    } else {
        ImGui::TextDisabled("Click a memory cell to view/edit. Type two hex digits to modify.");
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
    
    return memory_modified;
}

// =============================================================================
//  Global Instance
// =============================================================================

static MemoryGrid g_memory_grid;

MemoryGrid& get_memory_grid() {
    return g_memory_grid;
}
