// =============================================================================
// ui_premium.h - Premium UI styling layer for the Educational Binary IDE.
// =============================================================================
//
//  This module replaces the flat VS Code Dark+ look with a curated
//  "Cosmic Aurora" theme: deep navy/charcoal glassmorphic surfaces layered
//  with vibrant cyberpunk accents (electric cyan, magenta, neon green).
//
//  Responsibilities:
//    * SetupPremiumTheme()      - deep ImGuiStyle customization (colors,
//                                  padding, rounding, scrollbars, tabs).
//    * Icons namespace          - FontAwesome integration with graceful
//                                  text fallbacks when the font is missing.
//    * Animation helpers        - smooth hover lerp & looping pulse, keyed
//                                  by ImGuiID so it survives across frames.
//    * Custom widgets           - icon_button, gradient_button, status_dot,
//                                  glow_text, header_gradient, separator.
//    * Dockspace background     - animated aurora + faint grid for "WOW".
//
//  This header is self-contained: it only depends on imgui.h. The .cpp
//  adds <unordered_map> for the hover-state cache and <cmath> for the
//  easing functions.
//
// =============================================================================

#pragma once

#include "imgui.h"
#include <cstdint>

namespace ui {

// -----------------------------------------------------------------------------
//  Color palette.
//
//  All constants are packed as 0xAABBGGRR (ImU32 native ordering, NOT hex
//  web order). Use col::v() to convert to ImVec4 and col::with_alpha() to
//  clone a color with a different alpha. This keeps the rest of the code
//  free of float-tuple noise.
//
//  Palette concept: "Cosmic Aurora"
//    * Deep navy backgrounds (cooler than VS Code grey)
//    * Slightly elevated glass surfaces (subtle blue tint)
//    * Three accent colors used sparingly:
//        - CYAN  = primary actions, focus, PC pointer
//        - GREEN = success / running / "good"
//        - MAG   = attention / decompile / important
//    * AMBER / CORAL for warnings & errors
// -----------------------------------------------------------------------------
namespace col {

// Backgrounds (deep -> elevated). Format is 0xAABBGGRR (ImGui native).
constexpr ImU32 BG_DEEP     = 0xFF1A0E0A; // #0A0E1A near-black navy
constexpr ImU32 BG_MID      = 0xFF261711; // #111726
constexpr ImU32 SURFACE     = 0xFF38211A; // #1A2138 panel surface
constexpr ImU32 SURFACE_HI  = 0xFF47232B; // #232B47 elevated
constexpr ImU32 SURFACE_GLS = 0x5538211A; // semi-transparent glass surface

// Borders
constexpr ImU32 BORDER_SUB  = 0x14FFFFFF; // 8% white hairline
constexpr ImU32 BORDER_STR  = 0x40FFE500; // 25% cyan, used on focus

// Accents (vibrant cyberpunk)
constexpr ImU32 ACCENT_CYAN  = 0xFFFFE500; // #00E5FF electric cyan
constexpr ImU32 ACCENT_MAG   = 0xFF972EFF; // #FF2E97 hot magenta
constexpr ImU32 ACCENT_GREEN = 0xFF94FF00; // #00FF94 neon green
constexpr ImU32 ACCENT_AMBER = 0xFF00B8FF; // #FFB800 amber
constexpr ImU32 ACCENT_CORAL = 0xFF5747FF; // #FF4757 coral red
constexpr ImU32 ACCENT_PUR   = 0xFFE26B7C; // #7C6BE2 secondary purple

// Text
constexpr ImU32 TEXT_PRI = 0xFFF7EDE8; // #E8EDF7 cool near-white
constexpr ImU32 TEXT_SEC = 0xFFB5958B; // #8B95B5 muted blue-grey
constexpr ImU32 TEXT_DIS = 0xFF534940; // #404953 darker muted

// Convert ImU32 -> ImVec4
ImVec4 v(ImU32 packed);

// Return a copy of `packed` with the given alpha (0..1).
ImU32  with_alpha(ImU32 packed, float a);

// Mix two packed colors. t=0 -> a, t=1 -> b.
ImU32  mix(ImU32 a, ImU32 b, float t);

} // namespace col

// -----------------------------------------------------------------------------
//  FontAwesome icon integration.
//
//  Setup: download fa-solid-900.ttf from fontawesome.com and place it
//  somewhere the loader can find it (see FONTS.md). If the font is not
//  found, every Icon() call gracefully degrades to a text fallback, so
//  the application still runs and looks reasonable.
//
//  We use FontAwesome 6 free solid codepoints (the ones in the U+F000
//  Private Use Area). See https://fontawesome.com/search for the full list.
// -----------------------------------------------------------------------------
namespace icons {

// Try every well-known location and merge the FA range into the active
// font. Returns true if at least one path loaded successfully.
bool load(ImGuiIO& io);

// True iff load() succeeded at least once.
bool has();

// Convenience accessors. Each returns a UTF-8 string: the FA glyph if the
// font is loaded, otherwise a short ASCII fallback so the layout doesn't
// break. Combine with a text label using ICON_FA_PLAY " Run" style.
const char* play();        // \uf04b  ▶  fallback "+"
const char* step();        // \uf051  ▮▶ fallback ">"
const char* reset();       // \uf0e2  ↺  fallback "R"
const char* save();        // \uf0c7  💾 fallback "S"
const char* load_icon();   // \uf07c  📂 fallback "L"
const char* decompile();   // \uf085  ⚙  fallback "D"
const char* bug();         // \uf188     fallback "!"
const char* chip();        // \uf2db     fallback "#"
const char* branches();    // \uf126     fallback "%"
const char* memory();      // \uf538     fallback "M"
const char* code();        // \uf121     fallback "<>"
const char* terminal();    // \uf120     fallback "$"
const char* halt();        // \uf28d     fallback "X"
const char* bolt();        // \uf0e7     fallback "!"
const char* info();        // \uf05a     fallback "i"
const char* warn();        // \uf071     fallback "!"
const char* err();         // \uf06a     fallback "x"

} // namespace icons

// -----------------------------------------------------------------------------
//  Animation helpers.
//
//  ImGui is immediate mode, so we keep a tiny per-ID state cache in the
//  .cpp (an unordered_map<ImGuiID, float>) to track smoothed values.
//
//  Typical usage:
//      ImGui::PushID("btn");
//      float h = ui::anim::hover(ImGui::GetID("btn"));
//      ImGui::PushStyleColor(ImGuiCol_Button, ui::col::mix(base, hover, h));
//      ...
// -----------------------------------------------------------------------------
namespace anim {

// Per-frame bookkeeping. Call this once at the top of the frame.
void update();

// Smoothed hover factor (0..1) for the given ID. Lerps toward 1 when the
// item is hovered, toward 0 otherwise. Speed is fixed at ~12 Hz decay.
float hover(ImGuiID id);

// Looping pulse in [0,1] for steady glows. `speed` is cycles per second.
float pulse(float speed = 1.5f);

// Looping sine in [-1,1] for back-and-forth animations.
float sine(float speed = 1.5f);

// Cubic ease-out for one-shot progress (e.g., an animation that runs for
// `dur` seconds starting at `t0`).
float ease_out_cubic(float t);

} // namespace anim

// -----------------------------------------------------------------------------
//  Theme entry point. Call this once after ImGui::CreateContext().
// -----------------------------------------------------------------------------
void SetupPremiumTheme();

// -----------------------------------------------------------------------------
//  Custom widgets.
//
//  icon_button:
//    Draws a square-ish button with a large icon glyph centered. On hover
//    the button's background lerps from transparent to `accent` (with
//    alpha), and a subtle outer glow appears. Tooltip shown on hover.
//    `fallback` is rendered (smaller) if FontAwesome is unavailable.
//
//  gradient_button:
//    Standard text button but with a vertical gradient background. Used
//    for primary call-to-action buttons (e.g., "Run VM").
//
//  status_dot:
//    Colored circle with a soft outer glow. Used in the status bar.
//
//  glow_text / glow_text_colored:
//    Text rendered twice: once as a blurred shadow in the same hue, once
//    crisply on top. Cheap bloom-like effect.
//
//  header_gradient:
//    A section header rendered with a small accent bar to the left and
//    a gradient hairline to the right. Replaces plain ImGui::SeparatorHeader.
//
//  separator_gradient:
//    A 1px horizontal line that fades from transparent -> accent -> transparent.
//
//  pane_card:
//    Draws a rounded glass card behind subsequent content. Returns the
//    inner padding rect; caller is responsible for advancing cursor.
// -----------------------------------------------------------------------------
bool icon_button(const char* icon, const char* fallback, const char* tooltip,
                 ImVec2 size = ImVec2(0, 0),
                 ImU32 accent = col::ACCENT_CYAN);

bool gradient_button(const char* label, ImVec2 size,
                     ImU32 top, ImU32 bottom);

void status_dot(ImU32 color, float radius = 5.0f, bool pulsing = false);

void glow_text(const char* fmt, ...)             IM_FMTARGS(1);
void glow_text_colored(ImU32 color, const char* fmt, ...) IM_FMTARGS(2);

void header_gradient(const char* label,
                     ImU32 accent = col::ACCENT_CYAN);

void separator_gradient(ImU32 accent = col::ACCENT_CYAN, float alpha = 0.35f);

// -----------------------------------------------------------------------------
//  Dockspace background painter.
//
//  Call once per frame BEFORE ImGui::DockSpaceOverViewport(). It draws:
//    * A vertical gradient from BG_DEEP to BG_MID.
//    * A subtle animated "aurora" sweep in the top-right corner.
//    * A faint dot grid for tech ambiance.
//
//  The background is drawn directly via the foreground draw list so it
//  appears behind every docked window.
// -----------------------------------------------------------------------------
void draw_dockspace_background(ImVec2 pos, ImVec2 size);

} // namespace ui
