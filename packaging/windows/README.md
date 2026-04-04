# Windows build and installer

## Prerequisites

1. Install **MSYS2** in `C:\msys64`
2. Open the **MSYS2 UCRT64** terminal and install:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-multimedia
```

3. To generate the installer, install **Inno Setup 6**:
   - https://jrsoftware.org/isinfo.php

## One-command build

From PowerShell or `cmd.exe` in the repository root:

```bat
build_scripts\build-windows.bat
```

This generates a portable build in `build-ucrt\` with:
- the app icon embedded in `InfinityAudio.exe`
- Qt DLLs and plugins
- MinGW/MSYS2 runtime DLLs

## One-command installer

```bat
build_scripts\build-installer.bat
```

This generates:

```text
dist\InfinityAudio-setup-0.3.0.exe
```

## Files that should stay in Git

- `CMakeLists.txt`
- `CMakePresets.json`
- `resources.qrc`
- `app_icon.rc`
- `cmake/copy_runtime_deps.cmake`
- `packaging/icons/infinityaudio.ico`
- `packaging/windows/InfinityAudio.iss`
- `build_scripts/build-windows.bat`
- `build_scripts/build-installer.bat`
- `README.md`

## Files that should NOT go to Git

- `build-ucrt/`
- `dist/`
- any generated DLLs / installers / temporary build output
