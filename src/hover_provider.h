// =============================================================================
// hover_provider.h  —  Contextual Hover IntelliSense System
// =============================================================================
//
//  This module provides a VS Code-style hover documentation system for the
//  Binary IDE Premium. When the user hovers over an opcode, label, register,
//  or memory address in the editor, a polished tooltip appears with relevant
//  information.
//
//  Architecture:
//    * OpcodeDocs       - Centralized documentation database for all opcodes
//    * HoverContext     - Data describing what is being hovered
//    * HoverProvider    - Abstract base class for hover providers
//    * Concrete providers:
//        - OpcodeHoverProvider
//        - MemoryHoverProvider
//        - LabelHoverProvider
//        - RegisterHoverProvider
//    * HoverSystem      - Facade that coordinates all providers
// =============================================================================

#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Forward declarations
class TextEditor;
struct AppState;

// =============================================================================
//  Opcode Documentation Database
// =============================================================================

struct OpcodeInfo {
    std::string_view mnemonic;        // e.g., "LDA"
    std::string_view name;            // e.g., "Load Accumulator"
    std::string_view description;     // Full description
    std::string_view syntax;          // e.g., "LDA address"
    std::string_view flags;           // e.g., "Z, N" or "None"
    std::string_view category;        // e.g., "Data Transfer"
    std::string_view operand_info;    // e.g., "4-bit memory address (0-15)"
    std::string_view usage_notes;     // Optional additional notes
};

// Returns the complete opcode documentation database
const std::unordered_map<std::string_view, OpcodeInfo>& get_opcode_docs();

// Find opcode info by mnemonic (case-insensitive)
std::optional<OpcodeInfo> find_opcode_info(std::string_view mnemonic);

// =============================================================================
//  Hover Context
// =============================================================================

enum class HoverKind {
    None,
    Opcode,
    Label,
    Register,
    MemoryAddress,
    NumericLiteral,
    Directive
};

struct HoverContext {
    HoverKind kind = HoverKind::None;
    
    // Token text as it appears in the editor
    std::string token;
    
    // Line number (0-based) where the token is located
    int line = -1;
    
    // Column position (0-based) where the token starts
    int column = -1;
    
    // For memory addresses: the resolved address value
    std::optional<uint8_t> address_value;
    
    // For numeric literals: the parsed value
    std::optional<uint8_t> numeric_value;
    
    // Additional context-specific data
    std::string extra_info;
};

// =============================================================================
//  Hover Provider Interface
// =============================================================================

struct HoverResult {
    bool has_content = false;
    std::string title;          // Main heading (e.g., "LDA")
    std::string subtitle;       // Secondary heading (e.g., "Load Accumulator")
    std::string description;    // Main description text
    std::vector<std::pair<std::string, std::string>> sections; // Section title + content pairs
    
    bool empty() const { return !has_content; }
};

class HoverProvider {
public:
    virtual ~HoverProvider() = default;
    
    // Try to provide hover information for the given context.
    // Returns std::nullopt if this provider cannot handle this context.
    virtual std::optional<HoverResult> provide(const HoverContext& ctx, 
                                                const AppState& state) const = 0;
    
    // Human-readable name for debugging
    virtual const char* name() const = 0;
};

// =============================================================================
//  Concrete Hover Providers
// =============================================================================

class OpcodeHoverProvider : public HoverProvider {
public:
    std::optional<HoverResult> provide(const HoverContext& ctx,
                                        const AppState& state) const override;
    const char* name() const override { return "OpcodeHoverProvider"; }
};

class MemoryHoverProvider : public HoverProvider {
public:
    std::optional<HoverResult> provide(const HoverContext& ctx,
                                        const AppState& state) const override;
    const char* name() const override { return "MemoryHoverProvider"; }
};

class LabelHoverProvider : public HoverProvider {
public:
    std::optional<HoverResult> provide(const HoverContext& ctx,
                                        const AppState& state) const override;
    const char* name() const override { return "LabelHoverProvider"; }
};

class RegisterHoverProvider : public HoverProvider {
public:
    std::optional<HoverResult> provide(const HoverContext& ctx,
                                        const AppState& state) const override;
    const char* name() const override { return "RegisterHoverProvider"; }
};

// =============================================================================
//  Hover System Facade
// =============================================================================

class HoverSystem {
public:
    HoverSystem();
    
    // Detect what is being hovered in the editor
    HoverContext detect_hover(const TextEditor& editor) const;
    
    // Get hover information from all registered providers
    std::optional<HoverResult> get_hover_info(const HoverContext& ctx,
                                               const AppState& state) const;
    
    // Render the hover tooltip if there is content
    // Returns true if a tooltip was rendered
    bool render_tooltip(const HoverContext& ctx, const AppState& state) const;

private:
    std::vector<HoverProvider*> providers_;
};

// =============================================================================
//  Utility Functions
// =============================================================================

// Parse a token to determine if it's an opcode, label, address, etc.
HoverKind classify_token(std::string_view token);

// Extract numeric value from various formats (decimal, hex 0x, binary)
std::optional<uint8_t> parse_numeric_value(std::string_view token);

// Check if a token is a known opcode (case-insensitive)
bool is_opcode(std::string_view token);

// Check if a token looks like a label (ends with ':')
bool is_label(std::string_view token);

// Check if a token looks like a memory address (@N or @0xN)
bool is_memory_address(std::string_view token);

// Check if a token is a register name (A, PC, etc.)
bool is_register(std::string_view token);

// =============================================================================
//  Global Accessor
// =============================================================================

// Get the global hover system instance
const HoverSystem& get_hover_system();
