@echo off
REM MakeOpaax.bat

if "%1"=="" (
    echo ❌ Usage: MakeOpaax ^<project-name^>
    exit /b 1
)

set PROJECT_NAME=%1

REM Récupérer le chemin du script
set SCRIPT_DIR=%~dp0
set CREATOR_EXE=%SCRIPT_DIR%OpaaxCreator\OpaaxCreator.exe

REM Vérifier que OpaaxCreator existe
if not exist "%CREATOR_EXE%" (
    echo ❌ Error: OpaaxCreator.exe not found at %CREATOR_EXE%
    exit /b 1
)

REM Lancer le créateur avec le répertoire courant
"%CREATOR_EXE%" %PROJECT_NAME%

if errorlevel 1 (
    exit /b 1
)

exit /b 0