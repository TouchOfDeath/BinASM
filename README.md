# Binary IDE Premium — Build Guide

An educational Integrated Development Environment for an 8-bit virtual machine.
Built with C++17, Dear ImGui (docking branch), GLFW 3, and OpenGL 3.

## Quick Start (Replit)

```bash
# First build (downloads ImGui + ImGuiColorTextEdit via CMake FetchContent)
cd binary-ide
./build.sh

# Subsequent builds — incremental, fast
./build.sh

# Run (requires X11/VNC display in Replit)
./build.sh run

# Clean build directory
./build.sh clean
```

## Feature Set

| Feature | Status |
|---------|--------|
| Multi-tab code editor with syntax highlighting | ✅ |
| Mnemonic assembler (LDA/ADD/SUB/STA/LDI/JMP/JC/JZ/OUT/HLT) | ✅ |
| Macro preprocessor (#define with parameters) | ✅ |
| Live linting with red squiggly underlines | ✅ |
| Step / Run / Reset VM controls | ✅ |
| Interactive hex memory editor | ✅ |
| Decompiler (memory → .bincode source) | ✅ |
| Control Flow Graph visualiser | ✅ |
| Macro inspector (expanded source view) | ✅ |
| Explorer panel + file save/load | ✅ |
| "Cosmic Aurora" premium ImGui theme | ✅ |
| FontAwesome icon toolbar | ✅ (optional, see FONTS.md) |

## Architecture

```
main.cpp         — UI wiring, docking layout, render loop (presentation layer)
ui_premium.cpp   — Cosmic Aurora theme + animation helpers (presentation layer)
vm.cpp           — 8-bit CPU + 2-pass assembler (application + VM layer)
preprocessor.cpp — Pass-0 macro expander with line-map (assembler support)
decompiler.cpp   — Memory → .bincode + CFG builder (analysis layer)
hex_editor.cpp   — ImGui hex grid widget (presentation layer)
```

## Dependencies (auto-fetched by CMake)

| Library | Version | How |
|---------|---------|-----|
| Dear ImGui | docking branch | FetchContent |
| ImGuiColorTextEdit | latest | FetchContent |
| GLFW | system (Nix) | find_package |
| OpenGL | system | find_package |

## Optional Fonts

Place these in `binary-ide/fonts/` for the full "Cosmic Aurora" experience:

- `JetBrainsMono-Regular.ttf` — monospace editor font
- `fa-solid-900.ttf` — FontAwesome 6 Free Solid (toolbar icons)

See `FONTS.md` for download links. The app boots fine without them (falls back to
ImGui's built-in font and ASCII toolbar labels).

## Emscripten / WebAssembly (bonus objective)

Build for the browser:

```bash
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S binary-ide -B binary-ide/build/wasm \
    -DBINARY_IDE_EMSCRIPTEN=ON \
    -DCMAKE_BUILD_TYPE=Release
cmake --build binary-ide/build/wasm
# Serve binary-ide/build/wasm/BinaryIDE.html
```

Desktop remains the primary target. The Emscripten path compiles cleanly but
the full UI experience (docking, file I/O, local fonts) requires additional
Emscripten-specific adaptations beyond initial CMake support.
