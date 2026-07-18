// =============================================================================
// preprocessor.cpp - Implementation of the Pass-0 macro preprocessor.
// =============================================================================
//
//  See preprocessor.h for the architectural overview. This file implements:
//
//    1. `parseDefine()`        - parses a `#define NAME(params) body` line.
//    2. `splitBodyOnSemicolons()` - breaks a body on unquoted `;`.
//    3. `substituteParams()`   - replaces formal params with actual args.
//    4. `findMacroCall()`      - scans a line for a macro invocation.
//    5. `process()`            - top-level driver.
//
//  Why single-pass, non-recursive expansion?
//    * Predictability: the user can read the expanded output and trace
//      exactly one level of substitution. Multi-level recursion can hide
//      bugs in educational code.
//    * Termination: no risk of infinite expansion if two macros reference
//      each other.
//    * Simplicity: the line-number map stays trivial to compute.
//
//  If recursive expansion is ever needed, the cleanest way to add it is to
//  loop `process()` on its own output (with the line map composed) until a
//  fixpoint is reached.
//
// =============================================================================

#include "preprocessor.h"

#include <cctype>
#include <sstream>
#include <algorithm>

// -----------------------------------------------------------------------------
//  Constructor - no special setup needed.
// -----------------------------------------------------------------------------
Preprocessor::Preprocessor() = default;

// -----------------------------------------------------------------------------
//  Helpers - identifier characters and trimming.
// -----------------------------------------------------------------------------
bool Preprocessor::isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}
bool Preprocessor::isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
std::string Preprocessor::trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}

// -----------------------------------------------------------------------------
//  Parse a `#define` line.
//
//  Accepted forms (after the leading `#define `):
//     NAME              BODY         // object-like; params empty
//     NAME(p1,p2,...)   BODY         // function-like
//
//  The body is everything after the first whitespace following the name
//  (or, for function-like macros, after the closing paren). Leading and
//  trailing whitespace is stripped from the body.
//
//  Returns false on syntax error (e.g. unbalanced parens, missing name).
// -----------------------------------------------------------------------------
bool Preprocessor::parseDefine(const std::string& line, int line_num,
                               Macro& out, std::string& error_msg) const {
    // Strip the leading "#define" keyword.
    // We accept both "#define" and "# define" (rare but C-compatible).
    size_t p = 0;
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
    if (line.compare(p, 7, "#define") != 0) {
        error_msg = "expected '#define'";
        return false;
    }
    p += 7;
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;

    // Parse the macro name.
    if (p >= line.size() || !isIdentStart(line[p])) {
        error_msg = "missing macro name after #define";
        return false;
    }
    size_t name_start = p;
    while (p < line.size() && isIdentChar(line[p])) ++p;
    out.name = line.substr(name_start, p - name_start);

    // Optional parameter list - MUST immediately follow the name with no
    // whitespace, mirroring the C preprocessor rule that distinguishes
    // `FOO(x)` (function-like) from `FOO (x)` (object-like with body "(x)").
    if (p < line.size() && line[p] == '(') {
        out.is_function_like = true;
        ++p; // consume '('
        // Parse comma-separated identifiers until ')'.
        while (p < line.size() && line[p] != ')') {
            while (p < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[p]))) ++p;
            if (p < line.size() && line[p] == ')') break;
            if (p >= line.size() || !isIdentStart(line[p])) {
                error_msg = "invalid parameter in macro definition";
                return false;
            }
            size_t param_start = p;
            while (p < line.size() && isIdentChar(line[p])) ++p;
            out.params.push_back(line.substr(param_start, p - param_start));
            while (p < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[p]))) ++p;
            if (p < line.size() && line[p] == ',') ++p;
        }
        if (p >= line.size() || line[p] != ')') {
            error_msg = "unterminated parameter list in #define";
            return false;
        }
        ++p; // consume ')'
    } else {
        out.is_function_like = false;
    }

    // Skip whitespace, then the rest of the line is the body.
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
    out.body = trim(line.substr(p));
    out.definition_line = line_num;
    return true;
}

// -----------------------------------------------------------------------------
//  Split a macro body on `;` separators, but DO NOT split inside quoted
//  strings. This lets a macro body contain a `;` inside a DATA "..."
//  literal if the user really wants to.
// -----------------------------------------------------------------------------
std::vector<std::string> Preprocessor::splitBodyOnSemicolons(const std::string& body) {
    std::vector<std::string> parts;
    std::string current;
    bool in_string = false;

    for (size_t i = 0; i < body.size(); ++i) {
        char c = body[i];
        if (c == '"') {
            in_string = !in_string;
            current += c;
        } else if (c == ';' && !in_string) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty() || !parts.empty()) {
        parts.push_back(trim(current));
    }
    return parts;
}

// -----------------------------------------------------------------------------
//  Substitute formal parameters with actual arguments inside a body
//  fragment. Parameters are matched as standalone identifiers, so a
//  parameter named "x" will not match the "x" inside "ax" or "x1".
//
//  Quoted strings ("...") are skipped and never undergo substitution.
// -----------------------------------------------------------------------------
std::string Preprocessor::substituteParams(const std::string& fragment,
                                           const std::vector<std::string>& params,
                                           const std::vector<std::string>& args) {
    if (params.empty()) return fragment;

    std::string out;
    out.reserve(fragment.size());
    bool in_string = false;

    for (size_t i = 0; i < fragment.size(); ) {
        char c = fragment[i];

        // Toggle string mode on unescaped quotes.
        if (c == '"') {
            in_string = !in_string;
            out += c;
            ++i;
            continue;
        }

        // Inside a string: copy verbatim until the closing quote.
        if (in_string) {
            out += c;
            ++i;
            continue;
        }

        // Identifier start: try to match a parameter name.
        if (isIdentStart(c)) {
            size_t id_start = i;
            while (i < fragment.size() && isIdentChar(fragment[i])) ++i;
            std::string ident = fragment.substr(id_start, i - id_start);

            // Look up the identifier in the parameter list.
            auto it = std::find(params.begin(), params.end(), ident);
            if (it != params.end()) {
                size_t idx = static_cast<size_t>(std::distance(params.begin(), it));
                if (idx < args.size()) {
                    out += args[idx];
                } else {
                    out += ident; // Should not happen if args were validated.
                }
            } else {
                out += ident;
            }
            continue;
        }

        // Default: copy the character.
        out += c;
        ++i;
    }
    return out;
}

// -----------------------------------------------------------------------------
//  Parse an argument list of a function-like macro invocation.
//  `paren_pos` is the index of the opening '(' in `line`.
//  On success, returns the index just past the matching ')' and fills
//  `args` with the trimmed argument texts. Returns npos on error.
//
//  Arguments are split on commas at the top paren nesting level. Nested
//  parens inside an argument (e.g. `FOO(BAR(1, 2))`) are preserved as
//  part of the argument text.
// -----------------------------------------------------------------------------
size_t Preprocessor::parseArgList(const std::string& line, size_t paren_pos,
                                  std::vector<std::string>& args) {
    if (paren_pos >= line.size() || line[paren_pos] != '(') {
        return std::string::npos;
    }
    size_t i = paren_pos + 1;
    int depth = 1;
    std::string current;
    bool in_string = false;

    while (i < line.size() && depth > 0) {
        char c = line[i];
        if (c == '"') in_string = !in_string;

        if (!in_string) {
            if (c == '(') ++depth;
            else if (c == ')') {
                --depth;
                if (depth == 0) break;
            }
            else if (c == ',' && depth == 1) {
                args.push_back(trim(current));
                current.clear();
                ++i;
                continue;
            }
        }
        current += c;
        ++i;
    }

    if (depth != 0) return std::string::npos; // unbalanced

    // Push the final argument (or an empty string if there were zero args).
    std::string last = trim(current);
    if (!last.empty() || !args.empty()) {
        args.push_back(last);
    }
    return i + 1; // position just past the closing ')'
}

// -----------------------------------------------------------------------------
//  Find the first macro invocation in `line` at or after `start`.
//
//  On success:
//    * fills `m` with the matched Macro (copy),
//    * fills `args` with the actual arguments (empty for object-like),
//    * sets `call_start` / `call_end` to the byte range in `line` covered
//      by the invocation (including the arg list, if any).
//  Returns false if no invocation is found.
//
//  Matching rules:
//    * An identifier matches a macro name only if it is a *standalone*
//      identifier (not preceded by another identifier char and not part
//      of a longer identifier).
//    * For function-like macros, the character immediately after the name
//      MUST be '(' (whitespace allowed between name and '('). If it is not,
//      we treat the identifier as a plain token and do NOT expand.
//    * For object-like macros, any standalone occurrence of the name
//      expands to the body.
// -----------------------------------------------------------------------------
bool Preprocessor::findMacroCall(const std::string& line, size_t start,
                                 const std::vector<Macro>& macros,
                                 Macro& m, std::vector<std::string>& args,
                                 size_t& call_start, size_t& call_end) const {
    if (macros.empty()) return false;

    bool in_string = false;  // track double-quoted string literals

    for (size_t i = start; i < line.size(); ++i) {
        char c = line[i];

        // Toggle string mode on unescaped quotes. We do NOT honor C-style
        // backslash escapes here because the BinAsm language uses simple
        // single-char string literals (e.g. DATA "A"); a backslash inside
        // such a string would itself be a literal character.
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;  // skip everything inside a string literal

        if (!isIdentStart(c)) continue;
        // Reject if the previous char is an identifier char (we are in the
        // middle of a longer identifier).
        if (i > 0 && isIdentChar(line[i - 1])) continue;

        // Extract the identifier.
        size_t id_start = i;
        while (i < line.size() && isIdentChar(line[i])) ++i;
        std::string ident = line.substr(id_start, i - id_start);

        // Look up the identifier in the macro table.
        const Macro* match = nullptr;
        for (const auto& mc : macros) {
            if (mc.name == ident) { match = &mc; break; }
        }
        if (!match) continue;

        // Skip whitespace after the name.
        size_t j = i;
        while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j]))) ++j;

        if (match->is_function_like) {
            // Function-like macro: require an opening paren.
            if (j >= line.size() || line[j] != '(') continue;
            std::vector<std::string> parsed_args;
            size_t after = parseArgList(line, j, parsed_args);
            if (after == std::string::npos) continue;

            // Validate argument count (allow trailing empty for the common
            // "no args" case FOO()).
            size_t expected = match->params.size();
            if (parsed_args.size() != expected) {
                // Be lenient: if the macro takes 0 params and the user wrote
                // FOO(), accept it. Otherwise, skip expansion (the assembler
                // will surface a more useful error).
                if (!(expected == 0 && parsed_args.size() == 1 && parsed_args[0].empty())) {
                    continue;
                }
                parsed_args.clear();
            }

            m = *match;
            args = parsed_args;
            call_start = id_start;
            call_end = after;
            return true;
        } else {
            // Object-like macro: expand in place.
            m = *match;
            args.clear();
            call_start = id_start;
            call_end = i;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Top-level driver.
//
//  Algorithm:
//    1. Read the source line by line.
//    2. If a line begins (after whitespace) with `#define`, parse it as a
//       macro definition and store it. Emit a blank line so the expanded
//       line count matches the original where possible (helps readability
//       when debugging the preprocessor).
//    3. Otherwise, scan the line for macro invocations and expand them.
//       Each invocation may produce 0, 1, or many output lines (one per
//       `;`-separated body fragment). Every produced line maps back to the
//       *current* original line number, which is what the linting system
//       needs to point the red squiggle at the right editor line.
//    4. Lines with no macro calls are copied verbatim.
//
//  The resulting `line_map` has `line_map[expanded_line - 1] = original_line`
//  for every expanded line.
// -----------------------------------------------------------------------------
PreprocessedSource Preprocessor::process(const std::string& source) const {
    PreprocessedSource result;
    last_macros_.clear();

    // Collect macros first so that a macro defined later in the file is
    // still recognised when the file is processed top-to-bottom. We do a
    // *two-pass* approach:
    //    Pass A: walk the source, collect every `#define`.
    //    Pass B: walk the source again, expanding invocations.
    // This matches the C preprocessor semantics where macros are visible
    // from the point of definition onwards - but for an educational
    // assembler it is friendlier to make all macros globally visible,
    // which is what we do here.
    //
    // (If strict C-style visibility is ever needed, simply remove Pass A
    //  and inline macro collection into Pass B; the rest of the code
    //  remains unchanged.)
    std::vector<Macro> macros;

    // ---------- Pass A: collect macros ----------
    {
        std::istringstream iss(source);
        std::string line;
        int line_num = 0;
        while (std::getline(iss, line)) {
            ++line_num;
            // Detect `#define` at start of (trimmed) line. We do NOT strip
            // comments here, because the body of a macro could legitimately
            // contain a `#`... but in practice #define lines are short and
            // comment-free. If a comment exists, it becomes part of the body
            // and will be ignored by the assembler's lexer later.
            std::string trimmed = trim(line);
            if (trimmed.empty()) continue;
            if (trimmed.size() >= 7 && trimmed.compare(0, 7, "#define") == 0) {
                Macro m;
                std::string err;
                if (parseDefine(line, line_num, m, err)) {
                    macros.push_back(m);
                }
                // On error, we deliberately do NOT emit anything here;
                // the assembler will catch the malformed line in Pass B
                // and report a proper error with the original line number.
            }
        }
    }
    last_macros_ = macros;

    // ---------- Pass B: expand invocations ----------
    std::ostringstream out;
    std::istringstream iss(source);
    std::string line;
    int line_num = 0;

    while (std::getline(iss, line)) {
        ++line_num;

        // If this is a #define line, emit a blank placeholder so the
        // expanded text remains readable. The line map records the
        // original line, so any error on this blank line (which would
        // only happen if a future expansion landed here, which it cannot)
        // still points to the right place.
        std::string trimmed = trim(line);
        if (trimmed.size() >= 7 && trimmed.compare(0, 7, "#define") == 0) {
            out << "\n";
            result.line_map.push_back(line_num);
            continue;
        }

        // Repeatedly expand macro invocations in this line until no more
        // are found. We do NOT recursively expand the result of an
        // expansion (single-pass semantics, see file header).
        std::string current = line;
        std::vector<std::string> expanded_fragments;
        bool any_expansion = false;

        // We process the line left-to-right. When a macro call is found,
        // everything before it is one output fragment (which may itself
        // contain `;` separators), and the macro body (after param subst)
        // is spliced in. We then continue scanning from after the call.
        //
        // To keep the line-map simple, we split the *entire* (possibly
        // expanded) line on `;` at the very end. This means a macro body
        // containing `;` will produce multiple output lines that all map
        // back to the current original line.
        std::string assembled;
        size_t scan_pos = 0;
        while (true) {
            Macro m;
            std::vector<std::string> args;
            size_t call_start = 0, call_end = 0;
            if (!findMacroCall(current, scan_pos, macros, m, args, call_start, call_end)) {
                assembled.append(current, scan_pos, std::string::npos);
                break;
            }
            // Append the text before the call verbatim.
            assembled.append(current, scan_pos, call_start - scan_pos);
            // Substitute params into each body fragment.
            std::vector<std::string> body_parts = splitBodyOnSemicolons(m.body);
            for (size_t k = 0; k < body_parts.size(); ++k) {
                std::string frag = substituteParams(body_parts[k], m.params, args);
                assembled += frag;
                if (k + 1 < body_parts.size()) assembled += ";";
            }
            any_expansion = true;
            scan_pos = call_end;
        }

        // Now split `assembled` on `;` to produce the final output lines.
        // If no expansion happened, `assembled == line` and we still split
        // on `;` so multi-statement lines like `LDA 1; ADD 2` are properly
        // broken out (matches the assembler's one-instruction-per-line
        // expectation).
        std::vector<std::string> parts = splitBodyOnSemicolons(assembled);
        if (parts.empty()) {
            out << "\n";
            result.line_map.push_back(line_num);
        } else {
            for (const auto& part : parts) {
                out << part << "\n";
                result.line_map.push_back(line_num);
            }
        }
        (void)any_expansion; // currently unused beyond documentation
    }

    result.text = out.str();
    return result;
}
