@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM Opaax Engine — Build Script
REM
REM Usage:                                              (F5 / startup app in VS)
REM   build.bat                   -> Debug + Editor  (default)  -> SandboxEditor.exe
REM   build.bat release           -> Release, no editor, no imgui -> Sandbox.exe
REM   build.bat release-editor    -> RelWithDebInfo + Editor      -> SandboxEditor.exe
REM   build.bat fast [target]     -> incremental build of ONE target in the
REM                                  already-configured debug-editor tree
REM                                  (no reconfigure). Default target: Sandbox.
REM   build.bat test              -> build OpaaxTests (debug-editor) + run CTest
REM   build.bat bench             -> build OpaaxTests (RELEASE) + run the perf suite
REM                                  (doctest suite "perf", skipped everywhere else). Soft gate.
REM   build.bat clean             -> delete all build/ directories
REM
REM EXIT CODE / OUTPUT CONTRACT (read this — it is load-bearing):
REM   * The script ALWAYS ends with exactly one marker line:
REM         OPAAX_BUILD_OK    on success
REM         OPAAX_BUILD_FAIL  on any failure
REM     Grep for those. Do NOT parse localized "[ERROR]" text.
REM   * The process exit code is the REAL result (0 ok / 1 fail). The old
REM     'goto end' path swallowed failures to exit 0 — that is fixed here.
REM   * 'pause' is skipped automatically for non-interactive runs: define
REM     OPAAX_NO_PAUSE (or CI) and it will not block. Agents/CI: set OPAAX_NO_PAUSE=1.
REM =============================================================================

set "BUILD_RC=0"

set "PRESET=%1"
if "%PRESET%"=="" set "PRESET=debug-editor"

REM --- clean -------------------------------------------------------------------
if "%PRESET%"=="clean" (
    echo Cleaning build directories...
    if exist build rmdir /s /q build
    echo Done.
    goto ok
)

REM --- dispatch non-preset modes ----------------------------------------------
if "%PRESET%"=="test"  goto runtests
if "%PRESET%"=="fast"  goto fastbuild
if "%PRESET%"=="bench" goto benchmark

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
echo   fast [target]     Incremental single-target build (debug-editor, no reconfigure)
echo   test              Build OpaaxTests (debug-editor) + run CTest
echo   bench             Build OpaaxTests (release) + run the perf suite (soft gate)
echo   clean             Delete all build directories
goto fail

:valid
REM --- map preset -> multi-config build config ---------------------------------
REM The Visual Studio generator is MULTI-CONFIG, so CMAKE_BUILD_TYPE in the preset
REM is ignored — the compiled config comes from --config here. Without it, every
REM preset would silently build Debug.
set "CONFIG=Debug"
if "%PRESET%"=="release"        set "CONFIG=Release"
if "%PRESET%"=="release-editor" set "CONFIG=RelWithDebInfo"

echo.
echo [Opaax] Preset : %PRESET%  (config: %CONFIG%)
echo.

REM --- configure ---------------------------------------------------------------
echo Configuring...
cmake --preset %PRESET%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto fail
)

REM --- build -------------------------------------------------------------------
echo.
echo Building...
cmake --build build/%PRESET% --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed.
    goto fail
)

REM --- name the app this preset is FOR (VS startup project / what to launch) ----
set "PRIMARY=SandboxEditor.exe  (editor)"
if "%PRESET%"=="release" set "PRIMARY=Sandbox.exe  (runtime)"

echo.
echo [Opaax] Output: build\%PRESET%\bin\%CONFIG%\
echo [Opaax] Run:    %PRIMARY%   ^<- VS startup project for this preset
goto ok

REM --- fast: incremental single target, no reconfigure -------------------------
:fastbuild
set "TARGET=%2"
if "%TARGET%"=="" set "TARGET=Sandbox"
echo.
echo [Opaax] Fast build: target "%TARGET%" (debug-editor / Debug, no reconfigure)
echo.
if not exist build\debug-editor (
    echo [Opaax] debug-editor not configured yet — configuring once...
    cmake --preset debug-editor
    if errorlevel 1 (
        echo [ERROR] CMake configure failed.
        goto fail
    )
)
cmake --build build/debug-editor --config Debug --target %TARGET%
if errorlevel 1 (
    echo [ERROR] Fast build failed.
    goto fail
)
goto ok

REM --- bench: perf suite in RELEASE (representative numbers) -------------------
REM The perf cases are a doctest suite "perf" marked skip(), so they DON'T run in `test`/CI.
REM Here we build OpaaxTests release and run ONLY that suite with --no-skip. doctest returns
REM non-zero if a (loose) budget CHECK fails -> the soft gate fails the build.
:benchmark
echo.
echo [Opaax] Perf bench — configuring + building OpaaxTests (release)...
cmake --preset release
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto fail
)
cmake --build build/release --config Release --target OpaaxTests
if errorlevel 1 (
    echo [ERROR] Test build failed.
    goto fail
)
echo.
echo [Opaax] Running perf suite (release; skipped in test/CI)...
build\release\bin\Release\OpaaxTests.exe --test-suite=perf --no-skip=true
if errorlevel 1 (
    echo [ERROR] Perf gate FAILED — a budget was exceeded.
    goto fail
)
goto ok

REM --- test: build OpaaxTests (debug-editor / Debug) + run CTest ---------------
:runtests
echo.
echo [Opaax] Configuring (debug-editor)...
cmake --preset debug-editor
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto fail
)

echo.
echo [Opaax] Building OpaaxTests...
cmake --build build/debug-editor --config Debug --target OpaaxTests
if errorlevel 1 (
    echo [ERROR] Test build failed.
    goto fail
)

echo.
echo [Opaax] Running CTest...
ctest --test-dir build/debug-editor -C Debug --output-on-failure
if errorlevel 1 (
    echo [ERROR] CTest reported failures.
    goto fail
)
goto ok

REM --- unified exit points -----------------------------------------------------
:ok
echo.
echo [Opaax] Build complete.
echo OPAAX_BUILD_OK
set "BUILD_RC=0"
goto end

:fail
echo.
echo OPAAX_BUILD_FAIL
set "BUILD_RC=1"
goto end

:end
REM Skip the interactive pause for CI / agent / redirected-stdin runs.
if not defined OPAAX_NO_PAUSE if not defined CI pause
REM Carry the real result past endlocal (expanded before endlocal runs).
endlocal & exit /b %BUILD_RC%
