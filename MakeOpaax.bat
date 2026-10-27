@echo off
setlocal

REM =============================================================================
REM MakeOpaax.bat — create a new Opaax game project (current engine architecture).
REM
REM Usage:   MakeOpaax <ProjectName> [<Folder>]
REM
REM Generates <Folder>\<ProjectName>\ mirroring Sandbox (game module + runtime exe
REM + editor exe + tests) and registers it in <Folder>\CMakeLists.txt. Without a
REM folder, the game goes in this repository. Another folder is a game repository
REM of its own: when it has no CMakeLists.txt yet, it gets one that adds this
REM engine as a subdirectory, with presets (Docs/GameRepository.md). The skeleton
REM lives in OpaaxCreator\Templates\ and OpaaxCreator\Workspace\; OpaaxCreator.exe
REM instantiates it and is built here (Release) when missing or out of date.
REM =============================================================================

if "%~1"=="" (
    echo Usage: MakeOpaax ^<ProjectName^> [^<Folder^>]
    exit /b 1
)

set "PROJECT_NAME=%~1"
set "SCRIPT_DIR=%~dp0"
set "CREATOR_DIR=%SCRIPT_DIR%OpaaxCreator"
set "CREATOR_EXE=%CREATOR_DIR%\build\bin\Release\OpaaxCreator.exe"

REM "%SCRIPT_DIR%." : the trailing dot keeps the trailing backslash from escaping
REM the closing quote in the child's argv. A folder given with one gets it too.
set "WORKSPACE=%~2"
if "%WORKSPACE%"=="" set "WORKSPACE=%SCRIPT_DIR%"
if "%WORKSPACE:~-1%"=="\" set "WORKSPACE=%WORKSPACE%."

REM --- build the creator: configured once, rebuilt when its source changed ------
if not exist "%CREATOR_DIR%\build\CMakeCache.txt" (
    echo [Opaax] Configuring OpaaxCreator once...
    cmake -S "%CREATOR_DIR%" -B "%CREATOR_DIR%\build"
    if errorlevel 1 (
        echo [Opaax] ERROR: OpaaxCreator CMake configure failed.
        exit /b 1
    )
)
cmake --build "%CREATOR_DIR%\build" --config Release
if errorlevel 1 (
    echo [Opaax] ERROR: OpaaxCreator build failed.
    exit /b 1
)

REM --- generate the project in the workspace -----------------------------------
"%CREATOR_EXE%" "%PROJECT_NAME%" "%WORKSPACE%" "%SCRIPT_DIR%."
if errorlevel 1 exit /b 1

exit /b 0
