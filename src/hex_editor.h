// =============================================================================
// hex_editor.h - Professional-grade memory hex editor widget for ImGui.
// =============================================================================
//
//  Feature 3 of the IDE upgrade:
//    "Interactive Memory Hex Editor Pane"
//
//  Responsibilities:
//    * Display the VM's 16-byte memory array in a grid (default: 8 bytes
//      per row, configurable).
//    * Show three columns of information per byte:
//         - Address  (e.g. "0x04")
//         - Hex      (e.g. "1F")
//         - ASCII    (e.g. the character, or '.' for non-printable bytes)
//    * Allow the user to click on any hex cell to select it, then type
//      two hex digits to overwrite the byte in real time.
//    * Highlight the VM's current Program Counter (PC) so users can see
//      which byte will be executed next.
//    * Highlight bytes that were edited this frame (transient flash) so
//      the user gets visual feedback that the write landed.
//    * Expose a `lastEditedAddress()` query so callers (e.g. main.cpp)
//      can trigger a decompiler refresh when memory changes.
//
//  UX design:
//    * Each hex cell is rendered as an ImGui::Selectable with a fixed
//      width. When clicked, the cell becomes "selected" and the next
//      keystrokes are interpreted as hex digit input.
//    * Typing two valid hex digits commits the new byte value and
//      automatically advances the selection to the next cell (just like
//      HxD / Cheat Engine / 010 Editor).
//    * Backspace deletes the last typed digit (if mid-edit) or moves
//      selection to the previous cell (if not mid-edit).
//    * Arrow keys navigate the selection up/down/left/right.
//
//  Thread-safety:
//    The widget is not thread-safe; it must be called from the same
//    thread that owns the ImGui context (the main/UI thread).
//
// =============================================================================

#pragma once

#include <cstdint>
#include <string>

// Forward-declare ImGui types we need so this header can be included
// without pulling in the full ImGui header in some translation units.
// (Callers that actually render the widget will of course include imgui.h.)
struct ImVec2;

// -----------------------------------------------------------------------------
//  HexEditor widget.
//
//  Usage pattern (in your ImGui render loop):
//
//      static HexEditor hex;
//      if (hex.render("Memory Hex Editor", vm.memory, 16,
//                     /*pc_highlight=*/ vm.halted ? 0xFFu : vm.pc)) {
//          // Memory was edited this frame - refresh the decompiler.
//          decompiler.decompile(vm.memory);
//      }
//
// -----------------------------------------------------------------------------
class HexEditor {
public:
    HexEditor();

    // Renders the hex editor pane.
    //
    // Parameters:
    //   title          - ImGui window title (also used as ImGui ID seed).
    //   memory         - Pointer to the first byte of memory to edit.
    //   size           - Number of bytes to display (typically 16 for this VM).
    //   pc_highlight   - Address of the current PC, or 0xFF if no highlight.
    //
    // Returns:
    //   true if any byte was edited during this frame. The caller should
    //   use this signal to refresh dependent views (decompiler, etc.).
    bool render(const char* title, uint8_t* memory, int size,
                uint8_t pc_highlight = 0xFF);

    // Returns the address of the last byte edited this frame, or -1 if
    // none. Cleared at the start of each render() call.
    int lastEditedAddress() const { return last_edited_addr_; }

    // Configuration: number of bytes per row (default 8, valid 1..16).
    void setBytesPerRow(int bpr) { bytes_per_row_ = (bpr < 1) ? 1 : (bpr > 16 ? 16 : bpr); }
    int  bytesPerRow()     const { return bytes_per_row_; }

    // Configuration: whether to show the ASCII column (default true).
    void setShowAscii(bool show) { show_ascii_ = show; }
    bool showAscii()       const { return show_ascii_; }

    // Configuration: whether read-only mode is active (default false).
    // When true, the editor displays values but does not accept input.
    void setReadOnly(bool ro) { read_only_ = ro; }
    bool readOnly()     const { return read_only_; }

private:
    // Renders one row of the hex grid starting at `row_start_addr`.
    void renderRow(uint8_t* memory, int size, int row_start_addr,
                   uint8_t pc_highlight);

    // Renders a single hex cell. Returns true if the cell was clicked.
    bool renderHexCell(uint8_t* memory, int addr, uint8_t pc_highlight);

    // Renders a single ASCII cell (display-only).
    void renderAsciiCell(const uint8_t* memory, int addr, uint8_t pc_highlight);

    // Handles keyboard input for the currently selected cell.
    // Returns true if a byte was edited.
    bool handleKeyboard(uint8_t* memory, int size);

    // Helpers for hex digit parsing.
    static bool isHexDigit(char c);
    static int  hexDigitValue(char c);

    // ---------- State ----------
    int  selected_addr_;       // Currently selected byte index, or -1.
    int  last_edited_addr_;    // Byte edited this frame, or -1.
    int  edit_nibble_count_;   // 0, 1, or 2 - how many hex digits typed so far.
    uint8_t edit_buffer_;      // Accumulates the two typed hex digits.

    // ---------- Configuration ----------
    int  bytes_per_row_;       // Bytes per row (default 8).
    bool show_ascii_;          // Show ASCII column (default true).
    bool read_only_;           // Disable editing (default false).

    // ---------- Layout constants ----------
    static constexpr float kCellWidth  = 28.0f;  // pixels per hex cell
    static constexpr float kCellHeight = 18.0f;  // pixels per row
    static constexpr float kAddrWidth  = 50.0f;  // pixels for the address column
    static constexpr float kAsciiCellWidth = 16.0f;
};
