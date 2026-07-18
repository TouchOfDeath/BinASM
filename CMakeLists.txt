cmake_minimum_required(VERSION 3.18)
project(BinaryIDE CXX)

# ============================================================================
#  Language standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # -std=c++17, not -std=gnu++17

# ============================================================================
#  Build options
# ============================================================================
option(BINARY_IDE_EMSCRIPTEN "Build for WebAssembly with Emscripten" OFF)

# ============================================================================
#  Platform detection helpers
# ============================================================================
if(WIN32)
    set(BIDE_PLATFORM "Windows")
elseif(APPLE)
    set(BIDE_PLATFORM "macOS")
else()
    set(BIDE_PLATFORM "Linux")
endif()
message(STATUS "BinaryIDE target platform: ${BIDE_PLATFORM}")

# ============================================================================
#  Dependencies via FetchContent
# ============================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)               # show download progress on first build
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) # skip git-update on already-fetched sources

# ---------------------------------------------------------------------------
#  Dear ImGui  (docking branch for DockSpace support)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(imgui)

# ---------------------------------------------------------------------------
#  ImGuiColorTextEdit  (use local copy with modifications for hover support)
# ---------------------------------------------------------------------------
# Use the local version in libs/ImGuiColorTextEdit which has exposed methods
# needed for the hover IntelliSense feature.
set(IMGUI_COLOR_TEXT_EDIT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/ImGuiColorTextEdit")

# ============================================================================
#  Platform-specific graphics / windowing
# ============================================================================
if(NOT BINARY_IDE_EMSCRIPTEN)
    # ---- Native desktop build -----------------------------------------------

    # GLFW: prefer a system/nix install; fall back to fetching source.
    find_package(glfw3 3.3 QUIET)
    if(NOT glfw3_FOUND)
        message(STATUS "glfw3 not found via find_package — fetching from source")
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        # On Windows with MSVC, avoid the GLFW shared library so there are
        # no runtime DLL dependencies for a self-contained executable.
        if(WIN32)
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        endif()
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
            GIT_SHALLOW    ON
        )
        FetchContent_MakeAvailable(glfw)
    endif()

    find_package(OpenGL REQUIRED)

else()
    # ---- WebAssembly (Emscripten) build -------------------------------------
    message(STATUS "Emscripten build — using Emscripten ports for GLFW+GL")
endif()

# ============================================================================
#  Source files
# ============================================================================
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(APP_SOURCES
    src/main.cpp
    src/app_state.cpp
    src/panel_editor.cpp
    src/panel_explorer.cpp
    src/panel_vm_state.cpp
    src/panel_console.cpp
    src/panel_debug_assist.cpp
    src/panel_disasm.cpp
    src/panel_decompiled.cpp
    src/panel_macro.cpp
    src/fonts.cpp
    src/editor_tab.cpp
    src/disassembler.cpp
    src/panel_cfg.cpp
    src/panel_statusbar.cpp
    src/panel_cheatsheet.cpp
    src/ui_premium.cpp
    src/vm.cpp
    src/decompiler.cpp
    src/hex_editor.cpp
    src/preprocessor.cpp
    src/hover_provider.cpp
)

set(TEXTEDIT_SOURCES
    ${IMGUI_COLOR_TEXT_EDIT_SOURCE_DIR}/TextEditor.cpp
)

# ============================================================================
#  Executable target
#
#  On Windows, WIN32 marks the executable as a GUI subsystem app (no console
#  window). Remove WIN32 if you want a console for debugging on Windows.
# ============================================================================
if(WIN32)
    add_executable(BinaryIDE WIN32
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
        ${TEXTEDIT_SOURCES}
    )
else()
    add_executable(BinaryIDE
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
        ${TEXTEDIT_SOURCES}
    )
endif()

# ============================================================================
#  Include directories
# ============================================================================
target_include_directories(BinaryIDE PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${IMGUI_COLOR_TEXT_EDIT_SOURCE_DIR}
)

# ============================================================================
#  Compiler / preprocessor definitions
# ============================================================================
if(WIN32)
    target_compile_definitions(BinaryIDE PRIVATE
        # Target Windows Vista+ for modern API availability.
        _WIN32_WINNT=0x0600
        WIN32_LEAN_AND_MEAN     # exclude rarely-used parts of windows.h
        NOMINMAX                # prevent windows.h min/max macros from clobbering std::min/max
        _CRT_SECURE_NO_WARNINGS # suppress MSVC's secure-function deprecation warnings
    )
endif()

# ============================================================================
#  Compiler flags — per-compiler, per-platform
# ============================================================================

# --- Silence warnings for all third-party translation units -----------------
# Use a generator expression so the flag is correct regardless of compiler.
set_source_files_properties(
    ${IMGUI_SOURCES}
    ${IMGUI_COLOR_TEXT_EDIT_SOURCE_DIR}/TextEditor.cpp
    PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>"
)

# --- Application code warnings ----------------------------------------------
if(MSVC)
    target_compile_options(BinaryIDE PRIVATE
        /W3                     # Reasonable warning level (not /Wall which is very noisy)
        /WX-                    # Warnings are NOT errors (keeps third-party headers tolerable)
        /wd4267                 # 'size_t' → 'int' conversion (common in ImGui / TextEditor)
        /wd4244                 # 'double' / 'float' conversion
        /wd4305                 # 'double' to 'float' truncation
        /wd4996                 # Deprecated POSIX names (e.g. _access) — not used, but belt+suspenders
        /wd4100                 # Unreferenced formal parameter (ImGui/TextEditor heavy)
        /MP                     # Parallel compilation (significant speed-up on multi-core)
        /utf-8                  # Source and execution charset are UTF-8
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(BinaryIDE PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    # Apple Clang: suppress deprecated OpenGL API warnings (macOS marks the
    # entire GL API as deprecated in favour of Metal, but it still works fine).
    if(APPLE)
        target_compile_options(BinaryIDE PRIVATE -Wno-deprecated-declarations)
    endif()
endif()

# --- Optimization (Release builds) ------------------------------------------
# CMake already adds /O2 for MSVC in Release, and -O3 for GCC/Clang.
# We add -O2 explicitly for GCC/Clang only when the generator doesn't do it
# (e.g. Makefile generators with an explicit CMAKE_BUILD_TYPE).
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(BinaryIDE PRIVATE -O2)
    endif()
endif()

# ============================================================================
#  Link libraries — platform-specific
# ============================================================================
if(NOT BINARY_IDE_EMSCRIPTEN)

    if(WIN32)
        # OpenGL::GL resolves to opengl32.lib on Windows automatically.
        # gdi32 and shell32 are needed by GLFW on Windows.
        target_link_libraries(BinaryIDE PRIVATE
            glfw
            OpenGL::GL
            gdi32
            shell32
        )
        if(MSVC)
            # MSVC: link the C runtime statically for a self-contained executable.
            # (Optional — remove if you prefer the dynamic CRT.)
            # set_property(TARGET BinaryIDE PROPERTY
            #     MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()

    elseif(APPLE)
        # macOS: OpenGL lives inside a system framework; also need IOKit and
        # CoreVideo (GLFW dependencies on macOS) and Cocoa for the window system.
        target_link_libraries(BinaryIDE PRIVATE
            glfw
            OpenGL::GL
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
        )

    else()
        # Linux / other POSIX: link pthread, dl, and math.
        # (dl is needed for dlopen used by some OpenGL loaders;
        #  m is needed for std::sin / std::exp in the UI animation code.)
        target_link_libraries(BinaryIDE PRIVATE
            glfw
            OpenGL::GL
            pthread
            dl
            m
        )
    endif()

else()
    # Emscripten WebAssembly build.
    target_compile_options(BinaryIDE PRIVATE
        -s USE_GLFW=3
        -s USE_WEBGL2=1
        -s FULL_ES3=1
    )
    target_link_options(BinaryIDE PRIVATE
        -s USE_GLFW=3
        -s USE_WEBGL2=1
        -s FULL_ES3=1
        -s ALLOW_MEMORY_GROWTH=1
        -s WASM=1
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
    )
    set_target_properties(BinaryIDE PROPERTIES SUFFIX ".html")
endif()

# ============================================================================
#  Install rules (optional — useful for packaging)
# ============================================================================
install(TARGETS BinaryIDE DESTINATION bin)

# Install the fonts directory alongside the binary so font probing works
# when the app is run from the install prefix.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/fonts/
        DESTINATION bin/fonts
        OPTIONAL)   # non-fatal if the fonts/ directory doesn't exist yet
