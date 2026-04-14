@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM Opaax Engine — Build Script
REM
REM Usage:
REM   build.bat                   → Debug + Editor  (default)
REM   build.bat release           → Release, no editor, no imgui
REM   build.bat release-editor    → RelWithDebInfo + Editor
REM   build.bat clean             → supprime tous les dossiers build/
REM =============================================================================

set PRESET=%1
if "%PRESET%"=="" set PRESET=debug-editor

REM --- clean -------------------------------------------------------------------
if "%PRESET%"=="clean" (
    echo Cleaning build directories...
    if exist build rmdir /s /q build
    echo Done.
    goto end
)

REM --- validate preset ---------------------------------------------------------
if "%PRESET%"=="debug-editor"    goto valid
if "%PRESET%"=="release"         goto valid
if "%PRESET%"=="release-editor"  goto valid

echo [ERROR] Unknown preset: "%PRESET%"
echo.
echo Valid presets:
echo   debug-editor      Debug + Editor (default)
echo   release           Release, no editor
echo   release-editor    RelWithDebInfo + Editor
echo   clean             Delete all build directories
goto end

:valid
echo.
echo [Opaax] Preset : %PRESET%
echo.

REM --- configure ---------------------------------------------------------------
echo Configuring...
cmake --preset %PRESET%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto end
)

REM --- build -------------------------------------------------------------------
echo.
echo Building...
cmake --build build/%PRESET%
if errorlevel 1 (
    echo [ERROR] Build failed.
    goto end
)

REM --- output path -------------------------------------------------------------
echo.
echo [Opaax] Build complete.

if "%PRESET%"=="debug-editor"   echo Run: build\%PRESET%\bin\Debug\Game.exe
if "%PRESET%"=="release"        echo Run: build\%PRESET%\bin\Release\Game.exe
if "%PRESET%"=="release-editor" echo Run: build\%PRESET%\bin\RelWithDebInfo\Game.exe

:end
endlocal
pause