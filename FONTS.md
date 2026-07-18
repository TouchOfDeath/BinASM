# Font setup for the Premium UI

The "Cosmic Aurora" premium theme uses two font files. The application
will boot fine without them (it falls back to ImGui's built-in font and
ASCII labels for icon buttons), but for the full "WOW" experience you
should install both.

## 1. JetBrains Mono (main UI font)

A clean, programming-oriented monospace font. Used for every label,
editor glyph, and console line.

- **Download:** <https://www.jetbrains.com/lp/mono/>
- **Install location (pick any of these, the loader probes them in order):**
  - `fonts/JetBrainsMono-Regular.ttf`            (recommended, project-local)
  - `assets/fonts/JetBrainsMono-Regular.ttf`
  - `/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf` (system-wide on Linux)
- **Fallback:** if JetBrains Mono is not found, the loader will try
  DejaVu Sans Mono and Liberation Mono automatically.

## 2. FontAwesome 6 Free Solid (icon font)

Used for the toolbar icons (Run / Step / Reset / Save / Load / Decompile),
section headers, and status indicators. Without this font the toolbar
still works but renders ASCII fallback labels (`+`, `>`, `R`, `S`, ...).

- **Download:** <https://fontawesome.com/download> → "Free for the Web" ZIP
  → extract `webfonts/fa-solid-900.ttf`
- **Install location (probed in order):**
  - `fonts/fa-solid-900.ttf`                       (recommended, project-local)
  - `assets/fonts/fa-solid-900.ttf`
  - `/usr/share/fonts/truetype/font-awesome/fa-solid-900.ttf` (system-wide)
- **Loader behavior:** the FA glyphs are merged into the main UI font
  via `ImFontConfig::MergeMode = true` and a glyph range of
  `U+F000 .. U+F8FF` (the FontAwesome Private Use Area). This means you
  can write `"\uf04b Run"` and ImGui will render the FA glyph followed
  by ` Run` in JetBrains Mono, all from the same font handle.

## Project layout (recommended)

```
BinaryIDE/
├── CMakeLists.txt
├── FONTS.md                <-- this file
├── fonts/                  <-- create this folder
│   ├── JetBrainsMono-Regular.ttf
│   └── fa-solid-900.ttf
├── src/
│   ├── main.cpp
│   ├── ui_premium.h        <-- new premium UI header
│   ├── ui_premium.cpp      <-- new premium UI implementation
│   ├── vm.{h,cpp}
│   ├── decompiler.{h,cpp}
│   ├── preprocessor.{h,cpp}
│   └── hex_editor.{h,cpp}
└── libs/
    ├── glfw/
    ├── imgui/
    └── ImGuiColorTextEdit/
```

## Verifying installation

When you launch the IDE, the bottom-right of the status bar shows
`FontAwesome: READY` (or `TEXT` if the font wasn't found). The toolbar
icons will be rendered as proper glyphs when READY.

## Troubleshooting

**Q: I see tofu boxes (□) instead of icons.**
A: The FA font loaded but the glyph range was wrong. Ensure you're
using FontAwesome **6 Free Solid** (`fa-solid-900.ttf`), not the Regular
or Brands variants. The codepoints used (e.g. `\uf04b` for play) live
in the Free Solid subset.

**Q: The toolbar buttons show `+ > R S L D` instead of icons.**
A: FA wasn't found. Either copy the file to `fonts/fa-solid-900.ttf`
or one of the system paths. The loader prints nothing on success but
you can `printf` inside `ui::icons::load()` to debug which paths were
probed.

**Q: The text looks like ProggyClean (tiny bitmap font).**
A: No main font was found. Copy JetBrains Mono (or any of the
fallbacks) to one of the probed paths.
