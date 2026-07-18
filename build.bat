@echo off
REM =============================================================================
REM build.bat  --  Configure and build Binary IDE on Windows (MSVC or MinGW).
REM
REM Usage (from a Developer Command Prompt or PowerShell):
REM   build.bat              -- Debug build (default)
REM   build.bat release      -- Optimised release build
REM   build.bat clean        -- Remove the build directory and exit
REM   build.bat run          -- Build then launch the app
REM
REM Requirements:
REM   * CMake 3.18+ on PATH
REM   * One of:
REM       - Visual Studio 2019 / 2022 (MSVC)  — open a Developer Command Prompt
REM       - MinGW-w64 with GCC on PATH        -- add -G "MinGW Makefiles" to the
REM                                               cmake configure call below
REM
REM Linux / macOS users: use build.sh instead.
REM =============================================================================

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
REM Strip trailing backslash.
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=debug"

REM --- Handle "clean" ---------------------------------------------------------
if /i "%BUILD_TYPE%"=="clean" (
    echo [build.bat] Removing build directory...
    if exist "%SCRIPT_DIR%\build" rmdir /s /q "%SCRIPT_DIR%\build"
    echo [build.bat] Done.
    goto :eof
)

REM --- Map build type to CMake values -----------------------------------------
if /i "%BUILD_TYPE%"=="release" (
    set "CMAKE_BUILD_TYPE=Release"
    set "BUILD_DIR=%SCRIPT_DIR%\build\release"
) else (
    set "CMAKE_BUILD_TYPE=Debug"
    set "BUILD_DIR=%SCRIPT_DIR%\build\debug"
)

echo.
echo ============================================================
echo   Binary IDE  --  %CMAKE_BUILD_TYPE% build  (Windows)
echo   Source : %SCRIPT_DIR%
echo   Output : %BUILD_DIR%
echo ============================================================
echo.

REM --- Configure (only once) --------------------------------------------------
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [build.bat] Configuring with CMake ...
    cmake ^
        -S "%SCRIPT_DIR%" ^
        -B "%BUILD_DIR%" ^
        -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if errorlevel 1 (
        echo [build.bat] ERROR: CMake configuration failed.
        exit /b 1
    )
    echo.
)

REM --- Build ------------------------------------------------------------------
echo [build.bat] Building ...
cmake --build "%BUILD_DIR%" --config %CMAKE_BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [build.bat] ERROR: Build failed.
    exit /b 1
)

echo.
echo ============================================================
echo   Build succeeded!
echo   Debug   binary: %BUILD_DIR%\Debug\BinaryIDE.exe
echo   Release binary: %BUILD_DIR%\Release\BinaryIDE.exe
echo ============================================================
echo.

REM --- Optional: run after build ----------------------------------------------
if /i "%BUILD_TYPE%"=="run" (
    set "EXE=%BUILD_DIR%\%CMAKE_BUILD_TYPE%\BinaryIDE.exe"
    if not exist "!EXE!" set "EXE=%BUILD_DIR%\BinaryIDE.exe"
    if exist "!EXE!" (
        echo [build.bat] Launching BinaryIDE ...
        start "" "!EXE!"
    ) else (
        echo [build.bat] WARNING: Could not locate BinaryIDE.exe to launch.
        echo [build.bat] Look inside %BUILD_DIR% for the executable.
    )
)

endlocal
