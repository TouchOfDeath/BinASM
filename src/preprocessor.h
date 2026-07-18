// =============================================================================
// preprocessor.h - Pass-0 macro preprocessor for the BinAsm assembler.
// =============================================================================
//
//  Feature 2 of the IDE upgrade:
//    "Macro Preprocessor Subsystem"
//
//  Responsibilities:
//    * Run *before* the existing two-pass assembler (so it is "Pass 0").
//    * Recognise C-style parameterised macros:
//         #define ADD_TEN(x) LDA x; ADD 10d; STA x
//    * Expand every macro invocation in the source, substituting formal
//      parameters with the actual arguments supplied at the call site.
//    * Maintain an accurate line-number map so the assembler's error
//      diagnostics still point at the *original* source line in the editor,
//      not the expanded line. This is critical for the red squiggly
//      lint markers.
//
//  Design:
//    * The preprocessor is purely textual: it does not understand opcodes.
//      It only knows about `#define`, identifiers, parentheses, and commas.
//    * Macro expansion is single-pass (no recursive expansion). This avoids
//      infinite loops and matches typical educational assembler behaviour.
//      (Recursive expansion can be added later by re-running the expander
//      on the output until a fixpoint is reached.)
//    * The output is a `PreprocessedSource` containing:
//         - `text`: the expanded source text (newline-separated).
//         - `line_map`: maps a 1-based line number in `text` to the
//           1-based line number in the original source.
//      The assembler consults `line_map` when reporting errors.
//
//  Grammar recognised:
//     #define NAME             BODY        // object-like macro (no params)
//     #define NAME(p1, p2,...) BODY        // function-like macro
//     NAME                       // expansion of object-like macro
//     NAME(arg1, arg2, ...)      // expansion of function-like macro
//
//  Argument substitution rules:
//    * Inside BODY, every standalone identifier matching a formal parameter
//      is replaced with the corresponding actual argument text.
//    * The `;` separator inside BODY becomes a newline in the output, so
//      a macro body like `LDA x; ADD 10d; STA x` expands to three separate
//      assembly lines, all mapping back to the call-site line number.
//    * Quoted strings ("...") are preserved verbatim and never undergo
//      parameter substitution (matches C preprocessor behaviour).
//
// =============================================================================

#pragma once

#include <string>
#include <vector>

// -----------------------------------------------------------------------------
//  A parsed macro definition.
// -----------------------------------------------------------------------------
struct Macro {
    std::string              name;        // Identifier text, e.g. "ADD_TEN".
    std::vector<std::string> params;      // Formal parameter names (may be empty).
    std::string              body;        // Raw body text after the closing paren.
    int                      definition_line; // 1-based line in the original source.
    bool                     is_function_like; // True if params were declared.
};

// -----------------------------------------------------------------------------
//  Output of the preprocessor: the expanded source plus a line-number map.
// -----------------------------------------------------------------------------
struct PreprocessedSource {
    std::string              text;        // Expanded source text.
    std::vector<int>         line_map;    // line_map[i] = original line of output line (i+1).

    // Convenience: returns the original line number for a 1-based expanded
    // line. Clamps to the last known original line if out of range.
    int originalLine(int expanded_line_1based) const {
        if (line_map.empty()) return expanded_line_1based;
        int idx = expanded_line_1based - 1;
        if (idx < 0) idx = 0;
        if (idx >= (int)line_map.size()) idx = (int)line_map.size() - 1;
        return line_map[idx];
    }
};

// -----------------------------------------------------------------------------
//  The Preprocessor itself.
// -----------------------------------------------------------------------------
class Preprocessor {
public:
    Preprocessor();

    // Run Pass 0 on the given source text. Returns the expanded source
    // plus the line-number map.
    PreprocessedSource process(const std::string& source) const;

    // For debugging / UI display: returns the macros parsed from the last
    // call to `process()`. Note: because `process()` is const, this is
    // populated in a thread-local fashion; treat it as best-effort.
    const std::vector<Macro>& lastMacros() const { return last_macros_; }

private:
    // Parses a single `#define ...` line into a Macro. Returns true on
    // success. On failure, returns false and sets `error_msg`.
    bool parseDefine(const std::string& line, int line_num,
                     Macro& out, std::string& error_msg) const;

    // Splits a macro body on unquoted `;` characters. Each piece becomes
    // one line of the expanded output.
    static std::vector<std::string> splitBodyOnSemicolons(const std::string& body);

    // Substitutes formal parameters in a body fragment with actual args.
    static std::string substituteParams(const std::string& fragment,
                                        const std::vector<std::string>& params,
                                        const std::vector<std::string>& args);

    // Finds the first macro invocation in `line` starting at `start`.
    // On success returns the index of the invocation, fills `m`, `args`,
    // and `arg_span` (start and end offsets in `line`). Returns false if
    // no invocation is found.
    bool findMacroCall(const std::string& line, size_t start,
                       const std::vector<Macro>& macros,
                       Macro& m, std::vector<std::string>& args,
                       size_t& call_start, size_t& call_end) const;

    // Parses the argument list of a function-like macro invocation,
    // starting just AFTER the opening paren. Returns the position just
    // past the closing paren, or std::string::npos on error.
    static size_t parseArgList(const std::string& line, size_t paren_pos,
                               std::vector<std::string>& args);

    // Small utilities.
    static bool isIdentChar(char c);
    static bool isIdentStart(char c);
    static std::string trim(const std::string& s);

    // Mutable cache for `lastMacros()`. Marked mutable so `process()`
    // can remain const (it is logically a query, not a mutation).
    mutable std::vector<Macro> last_macros_;
};
