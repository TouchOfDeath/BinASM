#pragma once
// =============================================================================
// editor_tab.h  —  Editor tab data structure + factory.
//
// Each open file in the Binary Editor is represented by an EditorTab.
// The factory function make_editor_tab() applies the full language
// definition + "Cosmic Aurora" palette so every new tab is consistent.
// =============================================================================

#include "TextEditor.h"
#include <string>

// ---------------------------------------------------------------------------
//  Data type
// ---------------------------------------------------------------------------
struct EditorTab {
    std::string name;         // Display name (e.g., "program.bincode")
    std::string filepath;     // Absolute/relative path, or empty if unsaved
    TextEditor  editor;
    bool        dirty = false; // True if there are unsaved changes
};

// ---------------------------------------------------------------------------
//  Factory
// ---------------------------------------------------------------------------

// Create a new EditorTab with the Binary IDE language definition and
// Cosmic Aurora palette applied. `content` is set as the initial text.
EditorTab make_editor_tab(const std::string& name,
                           const std::string& filepath,
                           const std::string& content);
