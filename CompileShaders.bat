@echo off
setlocal EnableDelayedExpansion

REM === CONFIGURATION ===
set GLSLANG_VALIDATOR="%VULKAN_SDK%\Bin\glslangValidator.exe"
set SHADER_SRC=OpaaxGameEngine\Shaders
set SHADER_DST=Sandbox\Shaders

REM === Create output folder if it doesn't exist ===
if not exist %SHADER_DST% (
    mkdir %SHADER_DST%
)

REM === Compile all .vert, .frag, .comp ===
for %%F in (%SHADER_SRC%\*.vert %SHADER_SRC%\*.frag %SHADER_SRC%\*.comp) do (
    echo Compiling %%F...
    %GLSLANG_VALIDATOR% -V %%F -o %SHADER_DST%\%%~nxF.spv
)

echo.
echo Compilation done.
pause
