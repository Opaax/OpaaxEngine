@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM Opaax Engine — Build Script
REM
REM Usage:
REM   build.bat                   → Debug + Editor  (default)
REM   build.bat release           → Release, no editor, no imgui
REM   build.bat release-editor    → RelWithDebInfo + Editor
REM   build.bat test              → build OpaaxTests (debug-editor) + run CTest
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

REM --- test --------------------------------------------------------------------
if "%PRESET%"=="test" goto runtests

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
echo   test              Build OpaaxTests (debug-editor) + run CTest
echo   clean             Delete all build directories
goto end

:valid
REM --- map preset -> multi-config build config ---------------------------------
REM The Visual Studio generator is MULTI-CONFIG, so CMAKE_BUILD_TYPE in the preset
REM is ignored — the compiled config comes from --config here. Without it, every
REM preset would silently build Debug.
set CONFIG=Debug
if "%PRESET%"=="release"        set CONFIG=Release
if "%PRESET%"=="release-editor" set CONFIG=RelWithDebInfo

echo.
echo [Opaax] Preset : %PRESET%  (config: %CONFIG%)
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
cmake --build build/%PRESET% --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed.
    goto end
)

REM --- output path -------------------------------------------------------------
echo.
echo [Opaax] Build complete.
echo Run: build\%PRESET%\bin\%CONFIG%\Game.exe
goto end

REM --- test: build OpaaxTests (debug-editor / Debug) + run CTest ---------------
:runtests
echo.
echo [Opaax] Configuring (debug-editor)...
cmake --preset debug-editor
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto end
)

echo.
echo [Opaax] Building OpaaxTests...
cmake --build build/debug-editor --config Debug --target OpaaxTests
if errorlevel 1 (
    echo [ERROR] Test build failed.
    goto end
)

echo.
echo [Opaax] Running CTest...
ctest --test-dir build/debug-editor -C Debug --output-on-failure

:end
endlocal
pause