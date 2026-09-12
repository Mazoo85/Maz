@echo off
REM build_editor.bat - one command to turn the Maz Engine source code into the Editor program.
REM
REM For Windows. (Mac / Linux users: run tools/build_editor.sh instead.)
REM
REM What it does, in plain terms: it checks that the tools it needs are installed, "cooks" the
REM source code into a runnable program (this is called building), then tells you where the
REM program is and how to open it. You do NOT need to know how to code to run this - just
REM double-click it, or run it from a Command Prompt.
REM
REM If something needed is missing, it stops and tells you what to install (with links).

setlocal enabledelayedexpansion

REM Go to the project root (this script lives in the tools\ subfolder).
cd /d "%~dp0.."

echo.
echo === Maz Engine - Editor builder ===
echo Project folder: %CD%
echo.
echo == Checking your computer has the tools it needs ==

set MISSING=0

where cmake >nul 2>nul
if %ERRORLEVEL%==0 (
  echo [ok] CMake found
) else (
  echo [!] CMake is NOT installed.  Get it here: https://cmake.org/download/
  set MISSING=1
)

REM A C++ compiler comes with "Visual Studio" (the free Community edition is fine).
REM We detect Visual Studio using its own locator tool, "vswhere", so this works even
REM from a normal Command Prompt or a double-click (no special developer prompt needed).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set VSFOUND=0
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set VSFOUND=1
)
REM Also accept the case where a compiler is already on PATH (developer prompt).
where cl >nul 2>nul
if %ERRORLEVEL%==0 set VSFOUND=1

if "%VSFOUND%"=="1" (
  echo [ok] Visual Studio C++ tools found
) else (
  echo [!] Visual Studio with C++ tools was NOT found.
  echo     Install "Visual Studio Community" (free) and, in the installer, check
  echo     the "Desktop development with C++" box:
  echo     https://visualstudio.microsoft.com/downloads/
  set MISSING=1
)

where glslangValidator >nul 2>nul
if %ERRORLEVEL%==0 (
  echo [ok] Vulkan shader compiler found
) else (
  echo [!] Vulkan SDK is NOT installed (needed for graphics).
  echo     Get it here: https://vulkan.lunarg.com/sdk/home
  set MISSING=1
)

if "%MISSING%"=="1" (
  echo.
  echo Some tools are missing (see above^). Install them, then run this script again.
  pause
  exit /b 1
)

echo.
echo == Building the Editor (this can take a few minutes the first time^) ==
echo First run downloads a couple of helper libraries automatically - that's normal.
echo.

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if not %ERRORLEVEL%==0 (
  echo.
  echo The setup step failed. Scroll up for the reason; usually a missing tool.
  pause
  exit /b 1
)

cmake --build build --config Release --target editor
if not %ERRORLEVEL%==0 (
  echo.
  echo The build failed. Scroll up for the first error line.
  pause
  exit /b 1
)

echo.
echo === Done! ===
echo Your Editor program was built. Look for editor.exe inside the "build" folder
echo (commonly build\bin\Release\editor.exe or build\bin\editor.exe^).
echo.
echo Double-click that file to open the Editor.
echo.
pause
