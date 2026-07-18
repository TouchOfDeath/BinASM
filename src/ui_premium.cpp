// =============================================================================
// ui_premium.cpp - Implementation of the premium UI styling layer.
// =============================================================================
//
//  See ui_premium.h for the architectural overview.
//
//  Implementation notes:
//
//  1. Color packing. ImGui's ImU32 is 0xAABBGGRR (alpha, blue, green, red
//     - i.e. little-endian ARGB in memory). All constants in col:: are
//     pre-packed. with_alpha() rebuilds a packed color from any source
//     color and a new alpha.
//
//  2. Hover smoothing. We keep an unordered_map<ImGuiID, float> that
//     caches the current "hover progress" for every widget that asks for
//     one. Each frame we lerp toward 1.0 if hovered, else toward 0.0.
//     The lerp uses a framerate-independent factor so the perceived
//     "spring" is the same at 30 FPS and 144 FPS.
//
//  3. FontAwesome loading. We probe several well-known paths for
//     fa-solid-900.ttf. If found, we use MergeMode=true so the FA glyphs
//     are looked up via the SAME font handle as the main UI font. This
//     means you can write u8"\uf04b Run" and ImGui will render the FA
//     glyph followed by " Run" in the main font, no font-switching needed.
//
//  4. Custom button rendering. ImGui's Button() is fine but its hover
//     feedback is binary. For the toolbar we want a smooth glow, so we
//     draw the button background ourselves via ImDrawList and only use
//     InvisibleButton() for hit-testing. This is a well-known pattern.
//
// =============================================================================

#include "ui_premium.h"
#include "imgui.h"
#include "imgui_internal.h"  // for ImGuiStorage / some style accessors

#include <unordered_map>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <filesystem>  // std::filesystem::exists (C++17, all platforms)

namespace ui {

// =============================================================================
//  col - palette accessors
// =============================================================================

ImVec4 col::v(ImU32 c) {
    return ImVec4(
        ((c >>  0) & 0xFF) / 255.0f,
        ((c >>  8) & 0xFF) / 255.0f,
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >> 24) & 0xFF) / 255.0f
    );
}

ImU32 col::with_alpha(ImU32 c, float a) {
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    ImU32 ai = static_cast<ImU32>(a * 255.0f + 0.5f);
    return (c & 0x00FFFFFF) | (ai << 24);
}

ImU32 col::mix(ImU32 a, ImU32 b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int ar = (a >>  0) & 0xFF, ag = (a >>  8) & 0xFF,
        ab = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int br = (b >>  0) & 0xFF, bg = (b >>  8) & 0xFF,
        bb = (b >> 16) & 0xFF, ba = (b >> 24) & 0xFF;
    int r = static_cast<int>(ar + (br - ar) * t);
    int g = static_cast<int>(ag + (bg - ag) * t);
    int bl= static_cast<int>(ab + (bb - ab) * t);
    int al= static_cast<int>(aa + (ba - aa) * t);
    return (al << 24) | (bl << 16) | (g << 8) | r;
}

// =============================================================================
//  icons - FontAwesome loader
// =============================================================================

namespace icons {

static bool g_loaded = false;

bool has() { return g_loaded; }

bool load(ImGuiIO& io) {
    // Probe order: project-local first, then platform system paths.
    const char* paths[] = {
        // Project-local (works on all platforms; drop the file in fonts/)
        "fonts/fa-solid-900.ttf",
        "assets/fonts/fa-solid-900.ttf",
        "./fa-solid-900.ttf",
        "../fonts/fa-solid-900.ttf",
        // Linux system paths
        "/usr/share/fonts/truetype/font-awesome/fa-solid-900.ttf",
        "/usr/share/fonts/fontawesome/fa-solid-900.ttf",
        "/usr/share/fonts/TTF/fa-solid-900.ttf",
        "/usr/local/share/fonts/fa-solid-900.ttf",
        // macOS (Homebrew / manual install)
        "/usr/local/share/fonts/fa-solid-900.ttf",
        "/Library/Fonts/fa-solid-900.ttf",
        // Windows (common install locations)
        "C:/Windows/Fonts/fa-solid-900.ttf",
        nullptr
    };

    // FontAwesome 6 Free Solid uses the U+F000..U+F8FF Private Use Area.
    // We request this range so the atlas doesn't bloat with unused glyphs.
    static const ImWchar ranges[] = { 0xf000, 0xf8ff, 0 };

    for (int i = 0; paths[i]; ++i) {
        std::error_code ec;
        if (!std::filesystem::exists(paths[i], ec) || ec) continue;

        ImFontConfig cfg;
        cfg.MergeMode = true;          // merge into the previously-loaded font
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        // Slight vertical nudge: FA glyphs sit a hair high in most fonts.
        cfg.GlyphOffset.y = 1.0f;
        cfg.GlyphMinAdvanceX = 13.0f;

        ImFont* f = io.Fonts->AddFontFromFileTTF(paths[i], 15.0f, &cfg, ranges);
        if (f) {
            g_loaded = true;
            return true;
        }
    }
    return false;
}

// FA6 free-solid codepoints (UTF-8 encoded).
// Each glyph is 3 bytes in UTF-8: 0xEF 0x80 0xBB where 0xBB = code & 0x3F.
// Easier to just write them as u8 string literals.
//
// Fallbacks are ASCII (so the layout still works) - rendered with a small
// leading space so they align visually with the icon glyphs.

#define ICON(glyph, fb) \
    (g_loaded ? (glyph) : (fb))

const char* play()       { return ICON(u8"\xEF\x81\x8B", "+"); }   // f04b
const char* step()       { return ICON(u8"\xEF\x81\x91", ">"); }   // f051 step-forward
const char* reset()      { return ICON(u8"\xEF\x83\xA2", "R"); }   // f0e2 undo
const char* save()       { return ICON(u8"\xEF\x83\x87", "S"); }   // f0c7 floppy
const char* load_icon()  { return ICON(u8"\xEF\x81\xBC", "L"); }   // f07c folder-open
const char* decompile()  { return ICON(u8"\xEF\x82\x85", "D"); }   // f085 gears
const char* bug()        { return ICON(u8"\xEF\x86\x88", "!"); }   // f188 bug
const char* chip()       { return ICON(u8"\xEF\x8B\x9B", "#"); }   // f2db microchip
const char* branches()   { return ICON(u8"\xEF\x84\xA6", "%"); }   // f126 code-branch
const char* memory()     { return ICON(u8"\xEF\x95\xB8", "M"); }   // f538 memory
const char* code()       { return ICON(u8"\xEF\x84\xA1", "<>");}   // f121 code
const char* terminal()   { return ICON(u8"\xEF\x84\xA0", "$"); }   // f120 terminal
const char* halt()       { return ICON(u8"\xEF\x8A\x8D", "X"); }   // f28d ban
const char* bolt()       { return ICON(u8"\xEF\x83\xA7", "!"); }   // f0e7 bolt
const char* info()       { return ICON(u8"\xEF\x81\x9A", "i"); }   // f05a info-circle
const char* warn()       { return ICON(u8"\xEF\x81\xB1", "!"); }   // f071 exclamation-triangle
const char* err()        { return ICON(u8"\xEF\x86\xAA", "x"); }   // f06a exclamation-circle

#undef ICON

} // namespace icons

// =============================================================================
//  anim - smoothed hover / looping pulse
// =============================================================================

namespace anim {

// Persistent hover state per ImGuiID.
static std::unordered_map<ImGuiID, float> g_hover_state;

void update() {
    // Periodic GC: drop entries that have settled at 0 for a while.
    // (Skipped here for simplicity - the map is bounded by the number of
    // unique IDs we encounter, which is small.)
}

// Framerate-independent exponential lerp toward target.
// k = 1 - exp(-rate * dt). With rate=10, ~63% per 100ms.
static inline float damp(float current, float target, float rate, float dt) {
    float k = 1.0f - std::exp(-rate * dt);
    return current + (target - current) * k;
}

float hover(ImGuiID id) {
    float& state = g_hover_state[id];  // default-constructs to 0
    float target = ImGui::IsItemHovered() ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    state = damp(state, target, 12.0f, dt);
    return state;
}

float pulse(float speed) {
    // We don't key off any ID - this is a global clock pulse. Use
    // ImGui::GetTime() so it advances with the application clock.
    double t = ImGui::GetTime() * speed;
    // 0..1 triangle wave (smoother than square, easier on the eye than sine).
    double f = t - std::floor(t);
    return (float)(f < 0.5 ? (f * 2.0) : (2.0 - f * 2.0));
}

float sine(float speed) {
    return (float)std::sin(ImGui::GetTime() * speed * 6.28318530718);
}

float ease_out_cubic(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float t1 = t - 1.0f;
    return t1 * t1 * t1 + 1.0f;
}

} // namespace anim

// =============================================================================
//  Theme - SetupPremiumTheme()
// =============================================================================

void SetupPremiumTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    // =========================================================================
    //  Geometry — VS Code / JetBrains-inspired proportions.
    //
    //  Targets: slightly tighter than the original theme (which was roomy but
    //  wasteful at smaller resolutions), but still appreciably roomier than
    //  ImGui defaults. Fonts are typically loaded at 14-15 px, so frame
    //  padding is tuned to those sizes.
    // =========================================================================
    style.WindowPadding       = ImVec2(14.0f, 12.0f);
    style.FramePadding        = ImVec2(10.0f,  6.0f);
    style.CellPadding         = ImVec2( 6.0f,  4.0f);
    style.ItemSpacing         = ImVec2( 8.0f,  6.0f);
    style.ItemInnerSpacing    = ImVec2( 6.0f,  4.0f);
    style.IndentSpacing       = 16.0f;
    style.ScrollbarSize       = 12.0f;
    style.GrabMinSize         = 10.0f;
    style.DockingSeparatorSize = 2.0f;  // crisp, thin dock splitters

    // =========================================================================
    //  Rounding — deliberately tiered.
    //
    //  Windows:   8 px soft macOS-like corners.
    //  Frames:    5 px — visible but understated.
    //  Tabs:      6 px on top only (bottom is flat against the window border).
    //  Popups:    8 px — matches windows.
    //  Scrollbars:10 px — pill-shaped.
    // =========================================================================
    style.WindowRounding     =  8.0f;
    style.ChildRounding      =  6.0f;
    style.FrameRounding      =  5.0f;
    style.PopupRounding      =  8.0f;
    style.ScrollbarRounding  = 10.0f;
    style.GrabRounding       =  5.0f;
    style.TabRounding        =  6.0f;
    style.LogSliderDeadzone  =  4.0f;

    // =========================================================================
    //  Borders — layered visibility.
    //
    //  Window borders are drawn at 1 px (subtle hairline); they are critical
    //  for docked-panel separation. Frame borders are off — frames communicate
    //  via background color alone. Tab borders are also off — the active tab
    //  is identified by its elevated background color.
    // =========================================================================
    style.WindowBorderSize    = 1.0f;
    style.ChildBorderSize     = 1.0f;
    style.PopupBorderSize     = 1.0f;
    style.FrameBorderSize     = 0.0f;
    style.TabBorderSize       = 0.0f;

    // =========================================================================
    //  Layout tweaks
    // =========================================================================
    style.WindowTitleAlign       = ImVec2(0.0f, 0.5f); // left-aligned titles (VS Code)
    style.SelectableTextAlign    = ImVec2(0.0f, 0.5f);
    style.SeparatorTextAlign     = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize= 1.0f;
    style.SeparatorTextPadding   = ImVec2(16.0f, 4.0f);
    style.DisplaySafeAreaPadding = ImVec2(0, 0);

    // =========================================================================
    //  Anti-aliasing
    // =========================================================================
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style.AntiAliasedLines           = true;
    style.AntiAliasedLinesUseTex     = true;
    style.AntiAliasedFill            = true;
    style.CurveTessellationTol       = 1.0f;
    style.CircleTessellationMaxError = 0.25f;

    // =========================================================================
    //  Color palette — "Cosmic Aurora v2" (VS Code Dark + meets cyberpunk).
    //
    //  Design principles:
    //    • Deep navy background (#0D1117 / #010409 range) — darker than v1,
    //      more contrast for the aurora blobs.
    //    • Two tiers of panel surfaces: mid-layer (#131A27) for inactive
    //      panels, elevated (#1C2333) for focused/active panels.
    //    • Accents used sparingly: electric cyan for primary actions, neon
    //      green for success, amber for warnings, coral for errors.
    //    • All title bars use the mid-layer; the active title bar is slightly
    //      more elevated (SURFACE_HI) so focus is immediately readable.
    // =========================================================================
    auto set  = [&style](ImGuiCol idx, ImU32 packed) {
        style.Colors[idx] = col::v(packed);
    };
    auto seta = [&style](ImGuiCol idx, ImU32 packed, float a) {
        style.Colors[idx] = col::v(col::with_alpha(packed, a));
    };

    // -- Backgrounds --
    set (ImGuiCol_WindowBg,         col::with_alpha(col::SURFACE,    0.98f));
    set (ImGuiCol_ChildBg,          col::with_alpha(col::SURFACE,    0.55f));
    set (ImGuiCol_PopupBg,          col::with_alpha(col::SURFACE_HI, 0.98f));
    set (ImGuiCol_MenuBarBg,        col::with_alpha(col::BG_MID,     0.90f));
    set (ImGuiCol_DockingEmptyBg,   col::BG_DEEP);
    seta(ImGuiCol_DockingPreview,   col::ACCENT_CYAN, 0.35f);

    // -- Borders --
    // 9% white: visible against the dark backgrounds but not distracting.
    seta(ImGuiCol_Border,           0xFFFFFFFF, 0.09f);
    seta(ImGuiCol_BorderShadow,     0x00000000, 0.00f);

    // -- Text --
    set (ImGuiCol_Text,             col::TEXT_PRI);
    set (ImGuiCol_TextDisabled,     col::TEXT_DIS);
    set (ImGuiCol_TextSelectedBg,   col::with_alpha(col::ACCENT_CYAN, 0.28f));

    // -- Input frames --
    // Noticeably darker than the window background — creates clear depth
    // for input fields, code boxes, and child windows.
    set (ImGuiCol_FrameBg,          col::with_alpha(col::BG_DEEP,    0.90f));
    seta(ImGuiCol_FrameBgHovered,   col::ACCENT_CYAN, 0.08f);
    seta(ImGuiCol_FrameBgActive,    col::ACCENT_CYAN, 0.16f);

    // -- Title bars --
    // Inactive: just the mid-layer — visually recedes.
    // Active:   slightly more elevated — the focused panel stands out.
    set (ImGuiCol_TitleBg,          col::with_alpha(col::BG_MID,     1.00f));
    set (ImGuiCol_TitleBgActive,    col::with_alpha(col::SURFACE_HI, 1.00f));
    set (ImGuiCol_TitleBgCollapsed, col::with_alpha(col::BG_DEEP,    0.80f));

    // -- Scrollbars: pill-shaped, barely visible unless hovered --
    seta(ImGuiCol_ScrollbarBg,          0x00000000, 0.00f);
    seta(ImGuiCol_ScrollbarGrab,        0xFFFFFFFF, 0.08f);
    seta(ImGuiCol_ScrollbarGrabHovered, col::ACCENT_CYAN, 0.40f);
    seta(ImGuiCol_ScrollbarGrabActive,  col::ACCENT_CYAN, 0.60f);

    // -- Checks / sliders --
    set (ImGuiCol_CheckMark,        col::ACCENT_CYAN);
    set (ImGuiCol_SliderGrab,       col::ACCENT_CYAN);
    set (ImGuiCol_SliderGrabActive, col::ACCENT_MAG);

    // -- Buttons: transparent base, accent tint on hover --
    seta(ImGuiCol_Button,           0xFFFFFFFF, 0.04f);
    seta(ImGuiCol_ButtonHovered,    col::ACCENT_CYAN, 0.20f);
    seta(ImGuiCol_ButtonActive,     col::ACCENT_CYAN, 0.38f);

    // -- Selectables / list items / tree nodes --
    seta(ImGuiCol_Header,           col::ACCENT_CYAN, 0.12f);
    seta(ImGuiCol_HeaderHovered,    col::ACCENT_CYAN, 0.20f);
    seta(ImGuiCol_HeaderActive,     col::ACCENT_CYAN, 0.32f);

    // -- Separators --
    seta(ImGuiCol_Separator,        0xFFFFFFFF, 0.07f);
    seta(ImGuiCol_SeparatorHovered, col::ACCENT_CYAN, 0.35f);
    seta(ImGuiCol_SeparatorActive,  col::ACCENT_CYAN, 0.65f);

    // -- Tabs: VS Code-style flat inactive, slightly elevated active --
    // Inactive tabs are barely perceptible against the title bar.
    // Active tab is elevated to SURFACE_HI with a thin top highlight drawn
    // by the custom tab bar render path (built into ImGui).
    set (ImGuiCol_Tab,              col::with_alpha(col::BG_MID,     0.80f));
    seta(ImGuiCol_TabHovered,       col::ACCENT_CYAN, 0.15f);
    set (ImGuiCol_TabActive,        col::with_alpha(col::SURFACE_HI, 1.00f));
    set (ImGuiCol_TabUnfocused,     col::with_alpha(col::BG_MID,     0.60f));
    set (ImGuiCol_TabUnfocusedActive, col::with_alpha(col::SURFACE,  1.00f));

    // -- Resize grips --
    seta(ImGuiCol_ResizeGrip,       0xFFFFFFFF, 0.04f);
    seta(ImGuiCol_ResizeGripHovered,col::ACCENT_CYAN, 0.35f);
    seta(ImGuiCol_ResizeGripActive, col::ACCENT_CYAN, 0.65f);

    // -- Tables --
    set (ImGuiCol_TableHeaderBg,        col::with_alpha(col::SURFACE_HI, 1.00f));
    seta(ImGuiCol_TableBorderStrong,    0xFFFFFFFF, 0.10f);
    seta(ImGuiCol_TableBorderLight,     0xFFFFFFFF, 0.04f);
    seta(ImGuiCol_TableRowBg,           0x00000000, 0.00f);
    seta(ImGuiCol_TableRowBgAlt,        0xFFFFFFFF, 0.022f);

    // -- Plots --
    set (ImGuiCol_PlotLines,            col::ACCENT_CYAN);
    set (ImGuiCol_PlotLinesHovered,     col::ACCENT_MAG);
    set (ImGuiCol_PlotHistogram,        col::ACCENT_AMBER);
    set (ImGuiCol_PlotHistogramHovered, col::ACCENT_CORAL);

    // -- Misc --
    set (ImGuiCol_ModalWindowDimBg,      col::with_alpha(0xFF000000, 0.60f));
    set (ImGuiCol_NavHighlight,          col::ACCENT_CYAN);
    set (ImGuiCol_NavWindowingHighlight, col::with_alpha(col::ACCENT_CYAN, 0.25f));
    set (ImGuiCol_NavWindowingDimBg,     col::with_alpha(0xFF000000, 0.45f));
}

// =============================================================================
//  Custom widgets
// =============================================================================

bool icon_button(const char* icon, const char* fallback, const char* tooltip,
                 ImVec2 size, ImU32 accent) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const char* glyph = icons::has() ? icon : fallback;
    const ImGuiStyle& style = ImGui::GetStyle();

    // Default size: square, slightly larger than a text button.
    if (size.x <= 0.0f) size.x = 38.0f;
    if (size.y <= 0.0f) size.y = 32.0f;

    const ImGuiID id = window->GetID(glyph);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

    // Smooth hover factor for this specific button.
    float h = anim::hover(id);
    if (hovered) h = std::max(h, 0.6f);  // ensure immediate visual response

    // ---- Background fill: subtle by default, accent-tinted on hover. ----
    ImDrawList* dl = window->DrawList;
    float bg_alpha = 0.06f + 0.20f * h;
    ImU32 bg = col::with_alpha(accent, bg_alpha);
    dl->AddRectFilled(bb.Min, bb.Max, bg, style.FrameRounding);

    // ---- Outer glow on hover: a larger, more transparent rect behind. ----
    if (h > 0.05f) {
        ImVec2 glow_min(bb.Min.x - 6.0f, bb.Min.y - 6.0f);
        ImVec2 glow_max(bb.Max.x + 6.0f, bb.Max.y + 6.0f);
        // Multi-pass for a soft falloff.
        for (int i = 3; i >= 1; --i) {
            float expand = (float)i * 2.5f;
            ImU32 glow_col = col::with_alpha(accent, 0.05f * h * (4.0f - i));
            dl->AddRectFilled(
                ImVec2(bb.Min.x - expand, bb.Min.y - expand),
                ImVec2(bb.Max.x + expand, bb.Max.y + expand),
                glow_col, style.FrameRounding + expand);
        }
    }

    // ---- Border: thin accent line that brightens with hover. ----
    ImU32 border_col = col::with_alpha(accent, 0.30f + 0.50f * h);
    dl->AddRect(bb.Min, bb.Max, border_col, style.FrameRounding, 1.0f, 0);

    // ---- Pressed feedback: slight inward shift + brighter bg. ----
    float text_offset = held ? 1.0f : 0.0f;
    if (held) {
        dl->AddRectFilled(bb.Min, bb.Max,
                          col::with_alpha(accent, 0.32f),
                          style.FrameRounding);
    }

    // ---- Icon glyph centered. ----
    ImVec2 text_size = ImGui::CalcTextSize(glyph);
    ImVec2 text_pos(
        bb.GetCenter().x - text_size.x * 0.5f + text_offset,
        bb.GetCenter().y - text_size.y * 0.5f + text_offset
    );
    // Glow underlay for the icon (very subtle, scales with hover).
    if (h > 0.1f) {
        dl->AddText(ImVec2(text_pos.x + 0.5f, text_pos.y + 0.5f),
                    col::with_alpha(accent, 0.35f * h), glyph);
    }
    dl->AddText(text_pos, col::TEXT_PRI, glyph);

    // ---- Tooltip with smooth fade. ----
    if (tooltip && tooltip[0] && hovered) {
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
        ImGui::BeginTooltip();
        // Accent-colored bullet + text.
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImVec2 tp = ImGui::GetCursorScreenPos();
        tdl->AddCircleFilled(ImVec2(tp.x + 4, tp.y + 8), 3.0f, accent, 12);
        ImGui::Dummy(ImVec2(14, 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
        ImGui::PopStyleVar(2);
    }

    return pressed;
}

bool gradient_button(const char* label, ImVec2 size, ImU32 top, ImU32 bottom) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 label_size = ImGui::CalcTextSize(label);
    if (size.x <= 0.0f) size.x = label_size.x + style.FramePadding.x * 2.0f;
    if (size.y <= 0.0f) size.y = label_size.y + style.FramePadding.y * 2.0f;

    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
    float h = anim::hover(id);
    if (hovered) h = std::max(h, 0.6f);

    ImDrawList* dl = window->DrawList;

    // Vertical gradient fill.
    ImU32 top_col    = col::mix(top, 0xFFFFFFFF, h * 0.10f);
    ImU32 bot_col    = col::mix(bottom, 0xFF000000, h * 0.10f);
    // ImDrawList doesn't have a native gradient rect, so we approximate
    // by stacking ~6 horizontal strips.
    const int strips = 6;
    float strip_h = (bb.Max.y - bb.Min.y) / strips;
    for (int i = 0; i < strips; ++i) {
        float t0 = (float)i / strips;
        float t1 = (float)(i + 1) / strips;
        ImU32 c1 = col::mix(top_col, bot_col, t1);
        (void)t0; // t0 would be used for a proper gradient; c1 approximates it
        // Use the bottom color of this strip for the fill (cheap approximation).
        dl->AddRectFilled(
            ImVec2(bb.Min.x, bb.Min.y + i * strip_h),
            ImVec2(bb.Max.x, bb.Min.y + (i + 1) * strip_h),
            c1, (i == 0 || i == strips - 1) ? style.FrameRounding : 0.0f);
    }

    // Glow border on hover.
    if (h > 0.05f) {
        for (int i = 3; i >= 1; --i) {
            float expand = (float)i * 2.0f;
            dl->AddRect(
                ImVec2(bb.Min.x - expand, bb.Min.y - expand),
                ImVec2(bb.Max.x + expand, bb.Max.y + expand),
                col::with_alpha(top, 0.06f * h * (4.0f - i)),
                style.FrameRounding + expand, 1.0f, 0);
        }
    }

    // Top highlight (1px lighter line, gives the "polished" look).
    dl->AddRect(
        ImVec2(bb.Min.x + 1, bb.Min.y + 1),
        ImVec2(bb.Max.x - 1, bb.Min.y + 2),
        col::with_alpha(0xFFFFFFFF, 0.25f),
        style.FrameRounding, 1.0f, 0);

    // Border.
    dl->AddRect(bb.Min, bb.Max,
                col::with_alpha(top, 0.50f + 0.40f * h),
                style.FrameRounding, 1.0f, 0);

    // Label.
    ImVec2 text_pos(
        bb.GetCenter().x - label_size.x * 0.5f,
        bb.GetCenter().y - label_size.y * 0.5f + (held ? 1.0f : 0.0f)
    );
    dl->AddText(text_pos, col::TEXT_PRI, label);

    return pressed;
}

void status_dot(ImU32 color, float radius, bool pulsing) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    // Vertically center on the current line.
    float line_h = ImGui::GetTextLineHeight();
    ImVec2 center(pos.x + radius + 2.0f, pos.y + line_h * 0.5f);

    ImDrawList* dl = window->DrawList;

    float pulse_t = pulsing ? anim::pulse(2.0f) : 0.0f;
    float outer_r = radius + 3.0f + pulse_t * 4.0f;
    float outer_a = 0.40f - pulse_t * 0.25f;
    if (outer_a < 0.0f) outer_a = 0.0f;

    // Outer glow ring (multi-pass for soft falloff).
    if (pulsing) {
        for (int i = 3; i >= 1; --i) {
            float r = outer_r + (float)i * 1.5f;
            dl->AddCircleFilled(center, r,
                col::with_alpha(color, outer_a * 0.33f), 24);
        }
    } else {
        dl->AddCircleFilled(center, radius + 2.5f,
            col::with_alpha(color, 0.20f), 24);
    }

    // Solid dot.
    dl->AddCircleFilled(center, radius, color, 24);

    // Inner highlight (small white dot, top-left).
    dl->AddCircleFilled(
        ImVec2(center.x - radius * 0.35f, center.y - radius * 0.35f),
        radius * 0.30f,
        col::with_alpha(0xFFFFFFFF, 0.50f), 12);

    // Advance cursor (square footprint).
    ImGui::Dummy(ImVec2((radius + 4.0f) * 2.0f, line_h));
}

void glow_text(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::CalcTextSize(buf);

    // Three-pass shadow for a soft bloom.
    ImU32 shadow = col::with_alpha(col::ACCENT_CYAN, 0.35f);
    dl->AddText(ImVec2(pos.x - 1, pos.y),     shadow, buf);
    dl->AddText(ImVec2(pos.x + 1, pos.y),     shadow, buf);
    dl->AddText(ImVec2(pos.x,     pos.y - 1), shadow, buf);
    dl->AddText(ImVec2(pos.x,     pos.y + 1), shadow, buf);
    // Crisp top layer.
    dl->AddText(pos, col::TEXT_PRI, buf);

    ImGui::Dummy(size);
}

void glow_text_colored(ImU32 color, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::CalcTextSize(buf);

    // Soft shadow in the same hue.
    ImU32 shadow = col::with_alpha(color, 0.45f);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            dl->AddText(ImVec2(pos.x + dx, pos.y + dy), shadow, buf);
        }
    dl->AddText(pos, color, buf);
    ImGui::Dummy(size);
}

void header_gradient(const char* label, ImU32 accent) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImVec2 size = ImGui::CalcTextSize(label);
    float pad_y = 6.0f;
    float h = size.y + pad_y * 2.0f;

    // Reserve layout space.
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::ItemSize(ImVec2(0.0f, h), 0.0f);
    // Span full width of the content region.
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail < 200.0f) avail = 200.0f;

    ImDrawList* dl = window->DrawList;

    // Accent bar (3px wide, full label height).
    float bar_w = 3.0f;
    dl->AddRectFilled(
        ImVec2(pos.x, pos.y + 2.0f),
        ImVec2(pos.x + bar_w, pos.y + h - 2.0f),
        accent, 1.5f);

    // Label text.
    dl->AddText(
        ImVec2(pos.x + bar_w + 10.0f, pos.y + pad_y),
        col::TEXT_PRI, label);

    // Gradient hairline to the right of the label.
    float text_end_x = pos.x + bar_w + 10.0f + size.x + 14.0f;
    float line_y = pos.y + h * 0.5f;
    int segs = 24;
    for (int i = 0; i < segs; ++i) {
        float t0 = (float)i / segs;
        float t1 = (float)(i + 1) / segs;
        float x0 = text_end_x + (avail - text_end_x + pos.x) * t0;
        float x1 = text_end_x + (avail - text_end_x + pos.x) * t1;
        // Fade from accent (left) to transparent (right).
        ImU32 c = col::with_alpha(accent, 0.50f * (1.0f - t0));
        dl->AddLine(ImVec2(x0, line_y), ImVec2(x1, line_y), c, 1.0f);
    }
}

void separator_gradient(ImU32 accent, float alpha) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail < 10.0f) avail = 10.0f;
    float line_y = pos.y + 2.0f;

    ImGui::ItemSize(ImVec2(0.0f, 6.0f), 0.0f);

    ImDrawList* dl = window->DrawList;
    int segs = 32;
    for (int i = 0; i < segs; ++i) {
        float t0 = (float)i / segs;
        float t1 = (float)(i + 1) / segs;
        // Fade in from left, fade out to right.
        float a;
        if (t0 < 0.5f) a = t0 * 2.0f;
        else           a = (1.0f - t0) * 2.0f;
        ImU32 c = col::with_alpha(accent, alpha * a);
        dl->AddLine(
            ImVec2(pos.x + avail * t0, line_y),
            ImVec2(pos.x + avail * t1, line_y),
            c, 1.0f);
    }
}

// =============================================================================
//  Dockspace background painter
// =============================================================================

void draw_dockspace_background(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // =========================================================================
    //  Cosmic Aurora v2 background — improved layering.
    //
    //  Layer order (back to front):
    //    1. Solid base fill (BG_DEEP — deepest navy/charcoal).
    //    2. Subtle radial gradient from bottom-center: slightly lighter BG_MID
    //       giving a gentle "ambient light from below" effect.
    //    3. Aurora blobs (3 this time): cyan upper-right, magenta lower-left,
    //       a subtle purple mid-right. Each blob drifts independently.
    //    4. Dot grid overlay — very faint, adds tech texture.
    //    5. Top edge vignette — ensures the menu/title bar area stays dark.
    //    6. Bottom edge vignette — status bar feels grounded.
    // =========================================================================

    // --- 1. Solid base ---
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), col::BG_DEEP);

    // --- 2. Ambient radial from bottom-center ---
    // A large, very faint circle to make the center-bottom feel warmer.
    {
        float cx = pos.x + size.x * 0.50f;
        float cy = pos.y + size.y * 1.05f;   // slightly below the viewport
        float r  = size.y * 0.90f;
        const int passes = 10;
        for (int i = passes; i >= 1; --i) {
            float ri = r * (float)i / passes;
            float ai = 0.018f * (1.0f - (float)(i - 1) / passes);
            dl->AddCircleFilled(ImVec2(cx, cy), ri,
                col::with_alpha(col::BG_MID, ai), 48);
        }
    }

    // --- 3. Aurora blobs ---
    double t = ImGui::GetTime();

    // Helper: soft multi-pass radial glow.
    auto aurora_blob = [&](float cx, float cy, float r, ImU32 color,
                           int passes, float peak_alpha) {
        for (int i = passes; i >= 1; --i) {
            float ri = r * (float)i / passes;
            // Quartic falloff — much softer than linear, gives a "light leak" feel.
            float frac = (float)(i - 1) / passes;
            float ai   = peak_alpha * (1.0f - frac * frac * frac);
            dl->AddCircleFilled(ImVec2(cx, cy), ri,
                col::with_alpha(color, ai), 48);
        }
    };

    // Blob A — Cyan, upper-right, slower drift.
    float ax = pos.x + size.x * (0.76f + 0.08f * (float)std::sin(t * 0.17));
    float ay = pos.y + size.y * (0.22f + 0.06f * (float)std::cos(t * 0.13));
    aurora_blob(ax, ay, size.y * 0.52f, col::ACCENT_CYAN, 12, 0.042f);

    // Blob B — Magenta, lower-left, slightly faster drift.
    float bx = pos.x + size.x * (0.22f + 0.07f * (float)std::cos(t * 0.19));
    float by = pos.y + size.y * (0.82f + 0.05f * (float)std::sin(t * 0.21));
    aurora_blob(bx, by, size.y * 0.46f, col::ACCENT_MAG, 12, 0.038f);

    // Blob C — Purple accent, mid-right, subtle. Keeps the upper-right
    // from feeling too cold.
    float cx = pos.x + size.x * (0.62f + 0.05f * (float)std::sin(t * 0.11 + 1.2));
    float cy = pos.y + size.y * (0.45f + 0.04f * (float)std::cos(t * 0.14 + 0.8));
    aurora_blob(cx, cy, size.y * 0.30f, col::ACCENT_PUR, 10, 0.025f);

    // --- 4. Dot grid — 24 px spacing, 4% white ---
    const float grid    = 24.0f;
    const ImU32 dot_col = col::with_alpha(0xFFFFFFFF, 0.040f);
    // Snap grid origin to avoid jitter.
    float gx0 = pos.x - std::fmod(pos.x, grid);
    float gy0 = pos.y - std::fmod(pos.y, grid);
    for (float gy = gy0; gy < pos.y + size.y; gy += grid) {
        for (float gx = gx0; gx < pos.x + size.x; gx += grid) {
            if (gx < pos.x || gy < pos.y) continue;
            dl->AddCircleFilled(ImVec2(gx, gy), 1.0f, dot_col, 4);
        }
    }

    // --- 5. Top-edge vignette: keeps panels readable against title bars ---
    {
        const int v_strips = 16;
        for (int i = 0; i < v_strips; ++i) {
            float t0 = (float)i / v_strips;
            float t1 = (float)(i + 1) / v_strips;
            float a  = 0.55f * (1.0f - t0 * t0); // quadratic fade
            dl->AddRectFilled(
                ImVec2(pos.x, pos.y + size.y * t0 * 0.06f),
                ImVec2(pos.x + size.x, pos.y + size.y * t1 * 0.06f),
                col::with_alpha(0xFF000000, a));
        }
    }

    // --- 6. Bottom-edge vignette: grounds the status bar ---
    {
        const int v_strips = 10;
        for (int i = 0; i < v_strips; ++i) {
            float t0 = (float)i / v_strips;
            float t1 = (float)(i + 1) / v_strips;
            float a  = 0.40f * (1.0f - (1.0f - t1) * (1.0f - t1));
            dl->AddRectFilled(
                ImVec2(pos.x, pos.y + size.y * (0.96f + t0 * 0.04f)),
                ImVec2(pos.x + size.x, pos.y + size.y * (0.96f + t1 * 0.04f)),
                col::with_alpha(0xFF000000, a));
        }
    }
}

} // namespace ui
