// =============================================================================
// editor_tab.cpp  —  EditorTab factory implementation.
// =============================================================================

#include "editor_tab.h"

EditorTab make_editor_tab(const std::string& name,
                           const std::string& filepath,
                           const std::string& content) {
    EditorTab t;
    t.name     = name;
    t.filepath = filepath;

    // ---- Language definition ------------------------------------------------
    // Build a custom language definition based on C++ so we get:
    //   * Bracket matching, basic tokenisation.
    //   * '#' as the single-line comment character.
    //   * Binary opcode keywords highlighted in gold.
    //   * '@addr' address tokens in purple.
    //   * 'label:' tokens in cyan.
    //   * DATA / '...' data directives in coral.
    TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::CPlusPlus();
    lang.mKeywords.clear();
    lang.mIdentifiers.clear();
    lang.mPreprocIdentifiers.clear();
    lang.mSingleLineComment = "#";
    lang.mTokenRegexStrings.push_back({
        "0001|0010|0011|0100|1110|1111|0000|0101|0110|0111|1000",
        TextEditor::PaletteIndex::Keyword
    });
    lang.mTokenRegexStrings.push_back({
        "@[0-9]+|@0x[0-9a-fA-F]+",
        TextEditor::PaletteIndex::Identifier
    });
    lang.mTokenRegexStrings.push_back({
        "[a-zA-Z_][a-zA-Z0-9_]*:",
        TextEditor::PaletteIndex::KnownIdentifier
    });
    lang.mTokenRegexStrings.push_back({
        ":=",
        TextEditor::PaletteIndex::Punctuation
    });
    lang.mTokenRegexStrings.push_back({
        "DATA|\\.\\.\\.",
        TextEditor::PaletteIndex::PreprocIdentifier
    });
    t.editor.SetLanguageDefinition(lang);

    // ---- Cosmic Aurora palette ----------------------------------------------
    TextEditor::Palette palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Keyword]                 = 0xFFFFE500; // gold for opcodes
    palette[(int)TextEditor::PaletteIndex::Default]                 = 0xFFF7EDE8; // warm white
    palette[(int)TextEditor::PaletteIndex::Comment]                 = 0xFFB5958B; // muted brown
    palette[(int)TextEditor::PaletteIndex::Background]              = 0xFF1A0E0A; // deep dark
    palette[(int)TextEditor::PaletteIndex::Identifier]              = 0xFF972EFF; // purple for @addr
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier]         = 0xFF00B8FF; // cyan for labels
    palette[(int)TextEditor::PaletteIndex::PreprocIdentifier]       = 0xFFE26B7C; // coral for DATA
    palette[(int)TextEditor::PaletteIndex::Number]                  = 0xFF94FF00; // green for numbers
    palette[(int)TextEditor::PaletteIndex::String]                  = 0xFF00B8FF;
    palette[(int)TextEditor::PaletteIndex::CharLiteral]             = 0xFF94FF00;
    palette[(int)TextEditor::PaletteIndex::Punctuation]             = 0xFFB5958B;
    palette[(int)TextEditor::PaletteIndex::Selection]               = 0x55FFE500;
    palette[(int)TextEditor::PaletteIndex::ErrorMarker]             = 0x80FF4757;
    palette[(int)TextEditor::PaletteIndex::Breakpoint]              = 0xFFFF2E97;
    palette[(int)TextEditor::PaletteIndex::LineNumber]              = 0xFF534940;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFill]         = 0x26FFE500;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = 0x13FFE500;
    palette[(int)TextEditor::PaletteIndex::CurrentLineEdge]         = 0x60FFE500;
    t.editor.SetPalette(palette);

    t.editor.SetText(content);
    return t;
}
