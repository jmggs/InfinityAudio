@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI\"
pushd "%ROOT_DIR%" >nul || exit /b 1

set "MSYS2_UCRT64=C:\msys64\ucrt64\bin"
set "MSYS2_USR=C:\msys64\usr\bin"

if not exist "%MSYS2_UCRT64%\g++.exe" (
    echo [ERROR] MSYS2 UCRT64 nao foi encontrado em "%MSYS2_UCRT64%".
    echo Instala o MSYS2 e os pacotes indicados no README.
    popd >nul
    exit /b 1
)

set "PATH=%MSYS2_UCRT64%;%MSYS2_USR%;%PATH%"

echo [1/2] Configuring CMake preset ucrt64-release...
cmake --preset ucrt64-release
if errorlevel 1 (
    popd >nul
    exit /b 1
)

echo [2/2] Building InfinityAudio...
cmake --build --preset ucrt64-release
if errorlevel 1 (
    popd >nul
    exit /b 1
)

echo.
echo Build OK:
echo   %ROOT_DIR%build-ucrt\InfinityAudio.exe

popd >nul
exit /b 0
