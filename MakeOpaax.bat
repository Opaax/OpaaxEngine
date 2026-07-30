@echo off
setlocal

REM =============================================================================
REM MakeOpaax.bat — create a new Opaax game project (current engine architecture).
REM
REM Usage:   MakeOpaax <ProjectName>
REM
REM Generates <repo>\<ProjectName>\ mirroring Sandbox (game module + runtime exe
REM + editor exe) and registers it in the root CMakeLists.txt. The skeleton lives
REM in OpaaxCreator\Templates\; OpaaxCreator.exe instantiates it and is built
REM here on demand (Release) when missing.
REM =============================================================================

if "%~1"=="" (
    echo Usage: MakeOpaax ^<ProjectName^>
    exit /b 1
)

set "PROJECT_NAME=%~1"
set "SCRIPT_DIR=%~dp0"
set "CREATOR_DIR=%SCRIPT_DIR%OpaaxCreator"
set "CREATOR_EXE=%CREATOR_DIR%\build\bin\Release\OpaaxCreator.exe"

REM --- build the creator once, if its exe is missing ---------------------------
if not exist "%CREATOR_EXE%" (
    echo [Opaax] OpaaxCreator.exe not found - building it once...
    cmake -S "%CREATOR_DIR%" -B "%CREATOR_DIR%\build"
    if errorlevel 1 (
        echo [Opaax] ERROR: OpaaxCreator CMake configure failed.
        exit /b 1
    )
    cmake --build "%CREATOR_DIR%\build" --config Release
    if errorlevel 1 (
        echo [Opaax] ERROR: OpaaxCreator build failed.
        exit /b 1
    )
)

REM --- generate the project at the workspace root ------------------------------
REM "%SCRIPT_DIR%." : the trailing dot keeps the trailing backslash from escaping
REM the closing quote in the child's argv.
"%CREATOR_EXE%" "%PROJECT_NAME%" "%SCRIPT_DIR%."
if errorlevel 1 exit /b 1

exit /b 0
