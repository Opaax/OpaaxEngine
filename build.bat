@echo off
setlocal enabledelayedexpansion

REM =============================================================================
REM Opaax Engine — Build Script
REM
REM ONE PRESET, ONE APP. Each preset builds — and starts — exactly one executable:
REM
REM   build.bat                   -> Debug + Editor (default)     -> SandboxEditor.exe
REM   build.bat debug             -> Debug, no editor             -> Sandbox.exe
REM   build.bat release           -> Release, no editor, no imgui -> Sandbox.exe
REM
REM   build.bat run [preset]      -> build the preset, then LAUNCH its app (default
REM                                  preset: debug-editor). Same app VS's F5 starts.
REM   build.bat fast [target]     -> incremental build of ONE target in the
REM                                  already-configured debug-editor tree
REM                                  (no reconfigure). Default target: Sandbox.
REM   build.bat test              -> build OpaaxTests (debug-editor) + run CTest
REM   build.bat bench             -> build OpaaxTests (RELEASE) + run the perf suite
REM                                  (doctest suite "perf", skipped everywhere else). Soft gate.
REM   build.bat clean             -> delete all build directories
REM
REM EXIT CODE / OUTPUT CONTRACT (read this — it is load-bearing):
REM   * The script ALWAYS ends with exactly one marker line:
REM         OPAAX_BUILD_OK    on success
REM         OPAAX_BUILD_FAIL  on any failure
REM     Grep for those. Do NOT parse localized "[ERROR]" text.
REM   * Those markers are about the BUILD. A launched app's exit code is reported
REM     separately as OPAAX_RUN_EXIT=<n> so a crash is visible without redefining them.
REM   * The process exit code is the REAL result (0 ok / 1 fail). The old
REM     'goto end' path swallowed failures to exit 0 — that is fixed here.
REM   * 'pause' is skipped automatically for non-interactive runs: define
REM     OPAAX_NO_PAUSE (or CI) and it will not block. Agents/CI: set OPAAX_NO_PAUSE=1.
REM =============================================================================

set "BUILD_RC=0"
set "RUNAPP="

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

REM --- run: same build path, then launch the preset's app ----------------------
if not "%PRESET%"=="run" goto validate
set "RUNAPP=1"
set "PRESET=%2"
if "%PRESET%"=="" set "PRESET=debug-editor"

REM --- validate preset ---------------------------------------------------------
:validate
if "%PRESET%"=="debug-editor" goto valid
if "%PRESET%"=="debug"        goto valid
if "%PRESET%"=="release"      goto valid

echo [ERROR] Unknown preset: "%PRESET%"
echo.
echo Valid presets:
echo   debug-editor      Debug + Editor (default)   -^> SandboxEditor.exe
echo   debug             Debug, no editor           -^> Sandbox.exe
echo   release           Release, no editor         -^> Sandbox.exe
echo.
echo Other modes:
echo   run [preset]      Build the preset, then launch its app
echo   fast [target]     Incremental single-target build (debug-editor, no reconfigure)
echo   test              Build OpaaxTests (debug-editor) + run CTest
echo   bench             Build OpaaxTests (release) + run the perf suite (soft gate)
echo   clean             Delete all build directories
goto fail

:valid
call :map_preset

echo.
echo [Opaax] Preset : %PRESET%  (config: %CONFIG%, app: %PRIMARY%.exe)
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

set "BIN_DIR=build\%PRESET%\bin\%CONFIG%"

echo.
echo [Opaax] Output: %BIN_DIR%\
echo [Opaax] App:    %PRIMARY%.exe   ^<- VS startup project for this preset

if not defined RUNAPP goto ok

REM --- launch the preset's app -------------------------------------------------
REM CWD is the exe's own dir, matching VS_DEBUGGER_WORKING_DIRECTORY — relative
REM asset paths miss otherwise.
if not exist "%BIN_DIR%\%PRIMARY%.exe" (
    echo [ERROR] Not found: %BIN_DIR%\%PRIMARY%.exe
    goto fail
)

echo.
echo [Opaax] Launching %PRIMARY%.exe ...
pushd "%BIN_DIR%"
REM ".\" is required, not cosmetic: a bare name is a PATH lookup, and the CWD is not
REM searched when NoDefaultCurrentDirectoryInExePath is set (-^> 9009, not a crash).
".\%PRIMARY%.exe"
set "RUN_RC=%ERRORLEVEL%"
popd
echo.
echo OPAAX_RUN_EXIT=%RUN_RC%
goto ok

REM --- preset -> build config + primary app ------------------------------------
REM The Visual Studio generator is MULTI-CONFIG, so CMAKE_BUILD_TYPE in the preset
REM is ignored — the compiled config comes from --config here. Without it, every
REM preset would silently build Debug.
:map_preset
set "CONFIG=Debug"
set "PRIMARY=Sandbox"
if "%PRESET%"=="debug-editor" set "PRIMARY=SandboxEditor"
if "%PRESET%"=="release"      set "CONFIG=Release"
goto :eof

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
