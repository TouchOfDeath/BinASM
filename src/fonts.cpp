// =============================================================================
// fonts.cpp  —  Font loading implementation.
// =============================================================================

#include "fonts.h"
#include "ui_premium.h"   // ui::icons::load()

#include "imgui.h"
#include <filesystem>     // std::filesystem::exists (C++17, all platforms)

ImFont* g_editor_font = nullptr;

// ---------------------------------------------------------------------------
// probe_font_path — returns the first path in the null-terminated list that
// exists on the filesystem.  Uses std::filesystem::exists so it compiles and
// works on Linux, macOS, and Windows without POSIX headers.
// ---------------------------------------------------------------------------
const char* probe_font_path(const char** paths) {
    for (int i = 0; paths[i]; ++i) {
        std::error_code ec;
        if (std::filesystem::exists(paths[i], ec) && !ec) return paths[i];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------

void LoadFonts(ImGuiIO& io) {
    // ---- UI font: prefer proportional sans-serif ----------------------------
    const char* ui_paths[] = {
        // Project-local (highest priority — drop fonts here for any platform)
        "fonts/Inter-Regular.ttf",
        "fonts/Roboto-Regular.ttf",
        // macOS system fonts
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
        // Windows system fonts
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        // Linux / nix profile
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // ImGui bundled fallback (only present if libs/ exists)
        "libs/imgui/misc/fonts/Roboto-Medium.ttf",
        nullptr
    };
    const char* ui_path = probe_font_path(ui_paths);
    if (ui_path) {
        io.Fonts->AddFontFromFileTTF(ui_path, 15.0f);
    }
    // If no path found, ImGui will insert ProggyClean as font[0].
    // Ensure there is at least one entry before FontAwesome merge.
    if (io.Fonts->Fonts.empty()) {
        io.Fonts->AddFontDefault();
    }

    // ---- Editor font: monospace ---------------------------------------------
    const char* mono_paths[] = {
        // Project-local (highest priority)
        "fonts/JetBrainsMono-Regular.ttf",
        "fonts/FiraCode-Regular.ttf",
        "assets/fonts/JetBrainsMono-Regular.ttf",
        // macOS
        "/Library/Fonts/JetBrainsMono-Regular.ttf",
        "/Users/Shared/Fonts/JetBrainsMono-Regular.ttf",
        // Windows system fonts (monospace fallbacks)
        "C:/Windows/Fonts/consola.ttf",       // Consolas
        "C:/Windows/Fonts/cour.ttf",          // Courier New
        "C:/Windows/Fonts/lucon.ttf",         // Lucida Console
        // Windows: JetBrains Mono if user installed it
        "C:/Windows/Fonts/JetBrainsMono-Regular.ttf",
        // Nix profile (Replit)
        "/nix/var/nix/profiles/default/share/fonts/truetype/JetBrainsMono-Regular.ttf",
        // Linux system font paths
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/OTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/fira-code/FiraCode-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        nullptr
    };
    const char* mono_path = probe_font_path(mono_paths);
    if (mono_path) {
        g_editor_font = io.Fonts->AddFontFromFileTTF(mono_path, 14.5f);
    }
    // Fall back to the UI font if no mono found.
    if (!g_editor_font && !io.Fonts->Fonts.empty()) {
        g_editor_font = io.Fonts->Fonts[0];
    }
}
