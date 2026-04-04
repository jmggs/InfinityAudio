# ∞ InfinityAudio

**Professional continuous audio recorder — C++/Qt6, zero dependencies.**

Current release: **v0.3.0**

![InfinityAudio](docs/screenshot.png)

Record broadcast-quality audio continuously with configurable file segmentation, real-time dual-channel VU meters, and a built-in web remote control panel.

---

## What's new in v0.3.0

- **Solved bugs**
  - real **AIFF** recording support
  - real **16-bit / 44.1 kHz** recording support
  - real **24-bit / 96 kHz** recording support
- **Improved**
  - keyboard shortcuts for **Rec**, **Stop**, and **Monitor**

---

## Features

- **Continuous recording** — automatic WAV segment rotation (15 / 30 / 45 / 60 minutes)
- **Custom file prefix** — prepend a custom name before timestamp (e.g. `StudioA_20260404_113000.wav`)
- **High-quality formats** — WAV or AIFF, 16-bit 44.1 kHz / 24-bit 48 kHz / 24-bit 96 kHz
- **Real-time VU meters** — dual-channel (L/R) with dB readings
- **Audio monitor** — listen back through any output device while recording
- **Web remote panel** — control recording from any device on the network via browser
- **Audio streaming** — monitor live audio in the browser via a dedicated web monitor endpoint
- **Watchdog** — auto-restarts recording if anything goes wrong
- **Debian package** — `.deb` installer with desktop launcher and icon
- **Zero external dependencies** — no NDI SDK, no FFmpeg, no libav

---

## Web Remote Panel

Start the remote server from **Remote** → set a port and optional password.

Open `http://<machine-ip>:<port>` in any browser to:

| Endpoint | Description |
|---|---|
| `GET /` | Web control panel (HTML) |
| `GET /status` | JSON status (recording, filename, elapsed) |
| `GET /inputs` | JSON list of audio input devices |
| `GET /rec` | Start recording |
| `GET /stop` | Stop recording |
| `GET /set-input?device=NAME` | Switch input device |
| `GET /monitor?enabled=1\|0` | Toggle GUI monitor (speaker monitoring) |
| `GET /web-monitor?enabled=1\|0` | Toggle web stream monitor |
| `GET /audio.wav` | Live audio stream (Web Audio API) |
| `GET /settings` | Read remote settings (`prefix`, `chunkMinutes`) |
| `GET /set-settings?prefix=...&chunkMinutes=15\|30\|45\|60` | Update remote settings |

---

## Build

### Linux (Ubuntu / Debian)

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-multimedia-dev qt6-base-dev-tools libqt6network6

chmod +x build_scripts/build-linux.sh
./build_scripts/build-linux.sh

# Binary at:
./build-linux/InfinityAudio
```

### Build Debian package (.deb)

```bash
cd build-linux
cpack -G DEB
# Output: infinityaudio_0.3.0_amd64.deb
sudo dpkg -i infinityaudio_0.3.0_amd64.deb
```

### Windows (MSYS2 / UCRT64)

```bash
# In MSYS2 UCRT64 terminal:
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-multimedia
```

Then, from `PowerShell` or `cmd.exe` in the repository root:

```bat
build_scripts\build-windows.bat
```

Output:

```text
build-ucrt\InfinityAudio.exe
```

### Windows installer (Inno Setup)

Install **Inno Setup 6** and run:

```bat
build_scripts\build-installer.bat
```

Output:

```text
dist\InfinityAudio-setup-0.3.0.exe
```

### macOS

```bash
brew install qt cmake ninja
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-mac --parallel
```

---

## Project structure

```
InfinityAudio/
├── CMakeLists.txt
├── CMakePresets.json
├── main.cpp
├── wav_writer.h / .cpp        — WAV writer (24-bit, no FFmpeg)
├── recorder.h / .cpp          — QAudioSource capture + hourly rotation
├── watchdog.h / .cpp          — auto-restart watchdog
├── ui_widget.h / .cpp         — main window
├── vu_meter_widget.h / .cpp   — dual-channel VU meter (QPainter)
├── web_server.h / .cpp        — built-in HTTP remote control server
├── build_scripts/
│   ├── build-linux.sh         — Linux one-step build script
│   ├── build-windows.bat      — Windows portable build script
│   └── build-installer.bat    — Windows Inno Setup build script
├── packaging/
│   ├── infinityaudio.desktop  — Linux desktop launcher
│   ├── windows/
│   │   ├── InfinityAudio.iss  — Inno Setup installer script
│   │   └── README.md          — Windows packaging notes
│   └── icons/
│       ├── infinityaudio.svg  — app icon (source)
│       └── infinityaudio.ico  — Windows icon / installer icon
└── docs/
    └── screenshot.png
```

---

## Dependencies

| Library | Notes |
|---|---|
| **Qt 6.4+** | Core, Gui, Widgets, Multimedia, Network |
| **CMake 3.21+** | Build system |
| **Ninja** | Recommended (or Unix Make) |

No NDI SDK, no FFmpeg, no libav — zero extra dependencies.

---

## Settings

- **Input Device** — select any available audio input
- **Recording Folder** — destination for recorded files
- **Container** — WAV or AIFF
- **File Prefix** — optional name before timestamp
- **Chunk Duration** — 15 / 30 / 45 / 60 minutes
- **Audio Profile** — 16-bit 44.1 kHz / 24-bit 48 kHz / 24-bit 96 kHz
- **Remote** — configure port and password for the web panel

Settings are persisted via `QSettings` and restored on next launch.

---

## File naming

Files are named with optional prefix + timestamp:

- `PREFIX_YYYYMMDD_HHMMSS.wav` (when prefix is set)
- `YYYYMMDD_HHMMSS.wav` (when prefix is empty)

---

## License

MIT

---

> Built with C++17 and Qt6. Designed for broadcast and professional audio environments.
