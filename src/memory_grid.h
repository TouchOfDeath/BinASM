// =============================================================================
// memory_grid.h - Interactive Memory Grid Viewer with color-coded visualization.
// =============================================================================
//
//  This module provides an enhanced memory viewer that displays the VM's
//  16-byte memory as an interactive grid with:
//    - Color coding based on value ranges
//    - Binary visualization bars
//    - Direct click-to-edit functionality
//    - Value type indicators (code vs data)
// =============================================================================

#pragma once

#include <cstdint>

// Forward declarations
class VM;
struct ImVec2;
struct ImVec4;

// =============================================================================
//  Memory Grid Display Modes
// =============================================================================

enum class MemoryViewMode {
    Hex,        // Show hex values (0x00 - 0xFF)
    Decimal,    // Show decimal values (0 - 255)
    Binary,     // Show binary bars
    Mixed       // Show hex + ASCII
};

// =============================================================================
//  Color Coding Strategy
// =============================================================================

enum class ColorStrategy {
    None,           // No color coding
    ByValueRange,   // Color based on numeric value ranges
    ByInstruction,  // Color based on opcode categories
    ByUsage         // Color based on code vs data usage
};

// =============================================================================
//  Memory Grid Widget
// =============================================================================

class MemoryGrid {
public:
    MemoryGrid();
    
    // Render the memory grid panel
    // Returns true if memory was modified
    bool render(const char* title, VM& vm);
    
    // Configuration
    void set_view_mode(MemoryViewMode mode) { view_mode_ = mode; }
    MemoryViewMode view_mode() const { return view_mode_; }
    
    void set_color_strategy(ColorStrategy strategy) { color_strategy_ = strategy; }
    ColorStrategy color_strategy() const { return color_strategy_; }
    
    void set_show_binary_bars(bool show) { show_binary_bars_ = show; }
    bool show_binary_bars() const { return show_binary_bars_; }
    
    void set_show_ascii(bool show) { show_ascii_ = show; }
    bool show_ascii() const { return show_ascii_; }
    
    void set_columns(int cols) { columns_ = cols; }
    int columns() const { return columns_; }
    
    // Get the last edited address (-1 if none)
    int last_edited_address() const { return last_edited_addr_; }
    
    // Get the currently hovered address (-1 if none)
    int hovered_address() const { return hovered_addr_; }

private:
    // Render a single memory cell
    // Returns true if the cell was edited
    bool render_cell(VM& vm, int addr, bool is_pc, float cell_width);
    
    // Get color for a memory value based on current strategy
    ImVec4 get_value_color(uint8_t value, int addr, bool is_pc) const;
    
    // Get tooltip text for a memory address
    const char* get_tooltip_text(uint8_t value, int addr) const;
    
    // Draw a binary visualization bar
    void draw_binary_bar(ImVec2 pos, uint8_t value, float width, float height) const;
    
    // State
    int selected_addr_;
    int hovered_addr_;
    int last_edited_addr_;
    int edit_nibble_count_;
    uint8_t edit_buffer_;
    
    // Configuration
    MemoryViewMode view_mode_;
    ColorStrategy color_strategy_;
    bool show_binary_bars_;
    bool show_ascii_;
    int columns_;
};

// =============================================================================
//  Global Accessor
// =============================================================================

// Get the global memory grid instance
MemoryGrid& get_memory_grid();
