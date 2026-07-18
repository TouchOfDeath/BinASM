// =============================================================================
// hover_provider.cpp  —  Contextual Hover IntelliSense Implementation
// =============================================================================

#include "hover_provider.h"
#include "app_state.h"
#include "isa.h"
#include "TextEditor.h"
#include "ui_premium.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <imgui.h>

// =============================================================================
//  Opcode Documentation Database
// =============================================================================

namespace {

// Static storage for opcode documentation strings
// Using static constexpr to ensure zero runtime initialization cost

constexpr OpcodeInfo DOC_NOP = {
    "NOP",
    "No Operation",
    "Executes no operation. The processor simply advances to the next instruction.",
    "NOP",
    "None",
    "Control",
    "None",
    "Useful for timing adjustments or as placeholder during development."
};

constexpr OpcodeInfo DOC_LDA = {
    "LDA",
    "Load Accumulator",
    "Loads a value from the specified memory address into the accumulator register A.",
    "LDA address",
    "Z, N",
    "Data Transfer",
    "4-bit memory address (0-15)",
    "The Zero flag is set if the loaded value is 0. "
    "The Negative flag is set if bit 7 of the loaded value is 1."
};

constexpr OpcodeInfo DOC_ADD = {
    "ADD",
    "Add to Accumulator",
    "Adds the value at the specified memory address to the accumulator. "
    "The result is stored back in the accumulator.",
    "ADD address",
    "C, Z, N",
    "Arithmetic",
    "4-bit memory address (0-15)",
    "The Carry flag is set if the addition produces a result greater than 255. "
    "Overflow wraps around (mod 256)."
};

constexpr OpcodeInfo DOC_SUB = {
    "SUB",
    "Subtract from Accumulator",
    "Subtracts the value at the specified memory address from the accumulator. "
    "The result is stored back in the accumulator.",
    "SUB address",
    "C, Z, N",
    "Arithmetic",
    "4-bit memory address (0-15)",
    "The Carry flag indicates borrow. Overflow wraps around (mod 256)."
};

constexpr OpcodeInfo DOC_STA = {
    "STA",
    "Store Accumulator",
    "Stores the current value of the accumulator into the specified memory address.",
    "STA address",
    "None",
    "Data Transfer",
    "4-bit memory address (0-15)",
    "The accumulator value is preserved. Memory at the address is overwritten."
};

constexpr OpcodeInfo DOC_LDI = {
    "LDI",
    "Load Immediate",
    "Loads an immediate 8-bit value directly into the accumulator.",
    "LDI value",
    "Z, N",
    "Data Transfer",
    "8-bit immediate value (0-255)",
    "Unlike LDA, the operand is the actual value, not a memory address. "
    "The Zero and Negative flags are updated based on the loaded value."
};

constexpr OpcodeInfo DOC_JMP = {
    "JMP",
    "Unconditional Jump",
    "Unconditionally jumps to the specified memory address. "
    "The program counter is set to the target address.",
    "JMP address",
    "None",
    "Control Flow",
    "4-bit memory address (0-15)",
    "Execution continues from the target address on the next cycle."
};

constexpr OpcodeInfo DOC_JC = {
    "JC",
    "Jump if Carry",
    "Jumps to the specified memory address if the Carry flag is set.",
    "JC address",
    "None",
    "Control Flow",
    "4-bit memory address (0-15)",
    "If Carry is clear, execution continues with the next instruction."
};

constexpr OpcodeInfo DOC_JZ = {
    "JZ",
    "Jump if Zero",
    "Jumps to the specified memory address if the Zero flag is set.",
    "JZ address",
    "None",
    "Control Flow",
    "4-bit memory address (0-15)",
    "If Zero is clear, execution continues with the next instruction."
};

constexpr OpcodeInfo DOC_OUT = {
    "OUT",
    "Output",
    "Outputs the current value of the accumulator to the console. "
    "The value is displayed as a decimal number.",
    "OUT",
    "None",
    "I/O",
    "None",
    "The accumulator value is preserved. Useful for debugging and output."
};

constexpr OpcodeInfo DOC_HLT = {
    "HLT",
    "Halt",
    "Stops program execution. The VM enters a halted state and will not "
    "execute further instructions until reset.",
    "HLT",
    "None",
    "Control",
    "None",
    "Should be placed at the end of programs to prevent execution of data."
};

// Build the opcode documentation map
const std::unordered_map<std::string_view, OpcodeInfo> build_opcode_docs() {
    std::unordered_map<std::string_view, OpcodeInfo> docs;
    docs["NOP"] = DOC_NOP;
    docs["LDA"] = DOC_LDA;
    docs["ADD"] = DOC_ADD;
    docs["SUB"] = DOC_SUB;
    docs["STA"] = DOC_STA;
    docs["LDI"] = DOC_LDI;
    docs["JMP"] = DOC_JMP;
    docs["JC"]  = DOC_JC;
    docs["JZ"]  = DOC_JZ;
    docs["OUT"] = DOC_OUT;
    docs["HLT"] = DOC_HLT;
    return docs;
}

const auto OPCODE_DOCS = build_opcode_docs();

} // anonymous namespace

const std::unordered_map<std::string_view, OpcodeInfo>& get_opcode_docs() {
    return OPCODE_DOCS;
}

std::optional<OpcodeInfo> find_opcode_info(std::string_view mnemonic) {
    // Convert to uppercase for case-insensitive lookup
    std::string upper;
    upper.reserve(mnemonic.size());
    for (char c : mnemonic) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    
    auto it = OPCODE_DOCS.find(upper);
    if (it != OPCODE_DOCS.end()) {
        return it->second;
    }
    return std::nullopt;
}

// =============================================================================
//  Token Classification Utilities
// =============================================================================

namespace {

std::string to_upper(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

bool is_binary_literal(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c != '0' && c != '1') return false;
    }
    return true;
}

} // anonymous namespace

bool is_opcode(std::string_view token) {
    return find_opcode_info(token).has_value();
}

bool is_label(std::string_view token) {
    return !token.empty() && token.back() == ':';
}

bool is_memory_address(std::string_view token) {
    if (token.empty() || token[0] != '@') return false;
    if (token.size() == 1) return false;
    
    std::string_view num_part = token.substr(1);
    if (num_part.size() >= 2 && num_part[0] == '0' && 
        (num_part[1] == 'x' || num_part[1] == 'X')) {
        // Hex format: @0xNN
        return num_part.size() > 2;
    }
    // Decimal or binary format
    return true;
}

bool is_register(std::string_view token) {
    if (token.empty()) return false;
    std::string upper = to_upper(token);
    // Common register names for this VM
    return upper == "A" || upper == "PC" || upper == "ACCUMULATOR" ||
           upper == "CARRY" || upper == "ZERO" || upper == "FLAGS";
}

std::optional<uint8_t> parse_numeric_value(std::string_view token) {
    if (token.empty()) return std::nullopt;
    
    // Binary literal (e.g., "01010101")
    if (is_binary_literal(token)) {
        if (token.size() > 8) return std::nullopt;
        uint8_t value = 0;
        for (char c : token) {
            value = (value << 1) | (c - '0');
        }
        return value;
    }
    
    // Hex literal (e.g., "0xFF" or "@0xFF")
    if (token.size() >= 2 && token[0] == '0' && 
        (token[1] == 'x' || token[1] == 'X')) {
        try {
            size_t pos;
            unsigned long val = std::stoul(std::string(token), &pos, 16);
            if (val > 255) return std::nullopt;
            return static_cast<uint8_t>(val);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    // Handle @address format
    if (token[0] == '@') {
        return parse_numeric_value(token.substr(1));
    }
    
    // Decimal literal
    try {
        size_t pos;
        unsigned long val = std::stoul(std::string(token), &pos, 10);
        if (val > 255) return std::nullopt;
        return static_cast<uint8_t>(val);
    } catch (...) {
        return std::nullopt;
    }
}

HoverKind classify_token(std::string_view token) {
    if (token.empty()) return HoverKind::None;
    
    // Strip trailing colon for label check
    std::string_view base_token = token;
    if (is_label(token)) {
        base_token = token.substr(0, token.size() - 1);
    }
    
    // Check for opcodes first
    if (is_opcode(base_token)) {
        return HoverKind::Opcode;
    }
    
    // Check for labels
    if (is_label(token)) {
        return HoverKind::Label;
    }
    
    // Check for memory addresses
    if (is_memory_address(token)) {
        return HoverKind::MemoryAddress;
    }
    
    // Check for registers
    if (is_register(token)) {
        return HoverKind::Register;
    }
    
    // Check for numeric literals
    if (parse_numeric_value(token).has_value()) {
        return HoverKind::NumericLiteral;
    }
    
    // Check for directives
    std::string upper = to_upper(token);
    if (upper == "DATA" || upper == "...") {
        return HoverKind::Directive;
    }
    
    return HoverKind::None;
}

// =============================================================================
//  Opcode Hover Provider
// =============================================================================

std::optional<HoverResult> OpcodeHoverProvider::provide(
    const HoverContext& ctx, const AppState& /*state*/) const {
    
    if (ctx.kind != HoverKind::Opcode) {
        return std::nullopt;
    }
    
    auto info_opt = find_opcode_info(ctx.token);
    if (!info_opt) {
        return std::nullopt;
    }
    
    const OpcodeInfo& info = *info_opt;
    HoverResult result;
    result.has_content = true;
    result.title = std::string(info.mnemonic);
    result.subtitle = std::string(info.name);
    result.description = std::string(info.description);
    
    result.sections.push_back({"Syntax", std::string(info.syntax)});
    
    if (info.flags != "None" && !info.flags.empty()) {
        result.sections.push_back({"Flags", std::string(info.flags)});
    }
    
    result.sections.push_back({"Category", std::string(info.category)});
    
    if (info.operand_info != "None" && !info.operand_info.empty()) {
        result.sections.push_back({"Operand", std::string(info.operand_info)});
    }
    
    if (!info.usage_notes.empty()) {
        result.sections.push_back({"Notes", std::string(info.usage_notes)});
    }
    
    return result;
}

// =============================================================================
//  Memory Hover Provider
// =============================================================================

std::optional<HoverResult> MemoryHoverProvider::provide(
    const HoverContext& ctx, const AppState& state) const {
    
    if (ctx.kind != HoverKind::MemoryAddress && 
        ctx.kind != HoverKind::NumericLiteral) {
        return std::nullopt;
    }
    
    uint8_t address = 0;
    bool is_addr = false;
    
    if (ctx.address_value.has_value()) {
        address = *ctx.address_value;
        is_addr = true;
    } else if (ctx.numeric_value.has_value()) {
        address = *ctx.numeric_value;
        // For numeric literals, only show memory if it's in valid range
        is_addr = (address < 16);
    } else {
        return std::nullopt;
    }
    
    if (!is_addr || address >= 16) {
        return std::nullopt;
    }
    
    HoverResult result;
    result.has_content = true;
    result.title = "Memory Address";
    
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << static_cast<int>(address);
    result.subtitle = oss.str();
    
    uint8_t value = state.vm.memory[address];
    
    std::ostringstream desc_oss;
    desc_oss << "Decimal: " << static_cast<int>(address);
    result.description = desc_oss.str();
    
    // Current value section
    std::ostringstream val_oss;
    val_oss << "0x" << std::hex << std::uppercase << std::setw(2) 
            << std::setfill('0') << static_cast<int>(value);
    result.sections.push_back({"Current Value", val_oss.str()});
    
    // Decimal value
    result.sections.push_back({"As Decimal", std::to_string(value)});
    
    // Binary representation
    std::string binary;
    for (int i = 7; i >= 0; --i) {
        binary += ((value >> i) & 1) ? '1' : '0';
    }
    result.sections.push_back({"Binary", binary});
    
    // If VM is running/paused, show additional context
    if (!state.vm.halted) {
        if (address == state.vm.pc) {
            result.sections.push_back({"Status", "← Program Counter"});
        }
    }
    
    return result;
}

// =============================================================================
//  Label Hover Provider
// =============================================================================

std::optional<HoverResult> LabelHoverProvider::provide(
    const HoverContext& ctx, const AppState& /*state*/) const {
    
    if (ctx.kind != HoverKind::Label) {
        return std::nullopt;
    }
    
    // Extract label name (remove trailing colon)
    std::string label_name = ctx.token;
    if (!label_name.empty() && label_name.back() == ':') {
        label_name.pop_back();
    }
    
    HoverResult result;
    result.has_content = true;
    result.title = "Label";
    result.subtitle = label_name;
    result.description = "A symbolic name for a memory address.";
    
    result.sections.push_back({"Type", "Code Label"});
    result.sections.push_back({"Usage", "Used as jump/branch target"});
    
    return result;
}

// =============================================================================
//  Register Hover Provider
// =============================================================================

std::optional<HoverResult> RegisterHoverProvider::provide(
    const HoverContext& ctx, const AppState& state) const {
    
    if (ctx.kind != HoverKind::Register) {
        return std::nullopt;
    }
    
    std::string upper = to_upper(ctx.token);
    
    HoverResult result;
    result.has_content = true;
    
    if (upper == "A" || upper == "ACCUMULATOR") {
        result.title = "Accumulator (A)";
        result.subtitle = "8-bit general purpose register";
        result.description = "Primary register for arithmetic and logic operations.";
        
        std::ostringstream oss;
        oss << "Current Value: 0x" << std::hex << std::uppercase 
            << std::setw(2) << std::setfill('0') 
            << static_cast<int>(state.vm.accumulator);
        result.sections.push_back({"Value", oss.str()});
        result.sections.push_back({"Decimal", std::to_string(state.vm.accumulator)});
    } else if (upper == "PC") {
        result.title = "Program Counter (PC)";
        result.subtitle = "4-bit address register";
        result.description = "Points to the next instruction to execute.";
        
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase 
            << static_cast<int>(state.vm.pc);
        result.sections.push_back({"Value", oss.str()});
    } else if (upper == "CARRY") {
        result.title = "Carry Flag";
        result.subtitle = "Status flag";
        result.description = "Set when ADD produces a carry (result > 255) "
                            "or SUB requires a borrow.";
        result.sections.push_back({"Current", state.vm.carry_flag ? "Set" : "Clear"});
    } else if (upper == "ZERO") {
        result.title = "Zero Flag";
        result.subtitle = "Status flag";
        result.description = "Set when the accumulator is zero after an ALU operation.";
        result.sections.push_back({"Current", state.vm.zero_flag ? "Set" : "Clear"});
    } else {
        return std::nullopt;
    }
    
    return result;
}

// =============================================================================
//  Hover System Implementation
// =============================================================================

static OpcodeHoverProvider g_opcode_provider;
static MemoryHoverProvider g_memory_provider;
static LabelHoverProvider g_label_provider;
static RegisterHoverProvider g_register_provider;

HoverSystem::HoverSystem() {
    providers_.push_back(&g_opcode_provider);
    providers_.push_back(&g_memory_provider);
    providers_.push_back(&g_label_provider);
    providers_.push_back(&g_register_provider);
}

HoverContext HoverSystem::detect_hover(const TextEditor& editor) const {
    HoverContext ctx;
    
    // Get mouse position relative to the editor
    ImVec2 mouse_pos = ImGui::GetMousePos();
    
    // We need to access internal editor state to get hovered coordinates
    // Since TextEditor doesn't expose ScreenPosToCoordinates publicly for
    // external use, we'll use GetWordUnderCursor which internally handles this
    
    // Get the word under the cursor
    std::string word = editor.GetWordUnderCursor();
    
    if (word.empty()) {
        return ctx; // No token under cursor
    }
    
    // Get cursor position (which follows mouse for hover detection)
    auto coords = editor.GetCursorPosition();
    
    ctx.token = word;
    ctx.line = coords.mLine;
    ctx.column = coords.mColumn;
    ctx.kind = classify_token(word);
    
    // Extract additional context based on token type
    if (ctx.kind == HoverKind::MemoryAddress) {
        ctx.address_value = parse_numeric_value(word.substr(1)); // Skip '@'
    } else if (ctx.kind == HoverKind::NumericLiteral) {
        ctx.numeric_value = parse_numeric_value(word);
    }
    
    return ctx;
}

std::optional<HoverResult> HoverSystem::get_hover_info(
    const HoverContext& ctx, const AppState& state) const {
    
    for (const auto* provider : providers_) {
        auto result = provider->provide(ctx, state);
        if (result.has_value()) {
            return result;
        }
    }
    
    return std::nullopt;
}

bool HoverSystem::render_tooltip(const HoverContext& ctx, 
                                  const AppState& state) const {
    if (ctx.kind == HoverKind::None) {
        return false;
    }
    
    auto result_opt = get_hover_info(ctx, state);
    if (!result_opt || result_opt->empty()) {
        return false;
    }
    
    const HoverResult& result = *result_opt;
    
    ImGui::BeginTooltip();
    
    // Title (bold, larger)
    ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_CYAN));
    ImGui::TextUnformatted(result.title.c_str());
    ImGui::PopStyleColor();
    
    // Subtitle
    if (!result.subtitle.empty()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_SEC));
        ImGui::Text("— %s", result.subtitle.c_str());
        ImGui::PopStyleColor();
    }
    
    // Separator
    ui::separator_gradient(ui::col::ACCENT_CYAN, 0.3f);
    ImGui::Dummy(ImVec2(0, 4));
    
    // Description
    if (!result.description.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(result.description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 4));
    }
    
    // Sections
    for (const auto& [section_title, content] : result.sections) {
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::ACCENT_GREEN));
        ImGui::Text("%s:", section_title.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ui::col::v(ui::col::TEXT_PRI));
        ImGui::TextUnformatted(content.c_str());
        ImGui::PopStyleColor();
    }
    
    ImGui::EndTooltip();
    return true;
}

// Global hover system instance
static HoverSystem g_hover_system;

const HoverSystem& get_hover_system() {
    return g_hover_system;
}
