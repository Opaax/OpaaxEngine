@echo off
echo Building Opaax Engine...
echo.

REM Créer le dossier build s'il n'existe pas
if not exist build mkdir build

REM Aller dans le dossier build
cd build

REM Configurer CMake
echo Configuring...
cmake .. -G "Visual Studio 17 2022" -DOPAAX_EDITOR_SUPPORT=ON

REM Compiler
echo.
echo Building...
cmake --build . --config Debug

REM Retourner au dossier root
cd ..

echo.
echo Done! Run: build\bin\Debug\Game.exe
pause