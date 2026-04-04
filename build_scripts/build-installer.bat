@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI\"
pushd "%ROOT_DIR%" >nul || exit /b 1

call "%ROOT_DIR%build_scripts\build-windows.bat"
if errorlevel 1 (
    popd >nul
    exit /b 1
)

set "ISCC_EXE="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC_EXE if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC_EXE for /f "delims=" %%I in ('where iscc 2^>nul') do if not defined ISCC_EXE set "ISCC_EXE=%%I"

if not defined ISCC_EXE (
    echo [ERROR] Inno Setup 6 nao foi encontrado.
    echo Instala a partir de https://jrsoftware.org/isinfo.php
    popd >nul
    exit /b 1
)

echo [3/3] Building Inno Setup installer...
"%ISCC_EXE%" "%ROOT_DIR%packaging\windows\InfinityAudio.iss"
if errorlevel 1 (
    popd >nul
    exit /b 1
)

echo.
echo Installer OK:
echo   %ROOT_DIR%dist\InfinityAudio-setup-0.2.0.exe

popd >nul
exit /b 0
