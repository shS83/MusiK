# MusiK C++ / Qt6 — experimental performance comparison

**Status: partial native port, not a complete replacement for MusiK.**

Based on the supplied `musik.zip` (30 October 2026).

## Included

- Native Qt6 Widgets player shell, playlist loading/saving (M3U8), autoplay next, volume, audio decoding.
- Qt Multimedia decoded audio-buffer callback with a **simple amplitude envelope** as a placeholder. Original Python FFT beat/mid/treble analyzer is **not ported yet**.
- Native Qt OpenGL widget and the **original, unmodified GLSL shader files** for Chrome sphere, PsyTunnel and Hiss, including Hiss two-pass FBO.
- C++ Matrixia: freshly generated glyphs stay fixed at their cell, head is white and trail fades. Color switching by mouse click.
- FPS shown in visualization window title (approximate rendering FPS).

## Not yet included

- Laser Nexus / Orbit Streams / Glyph Waves / Glyph Spin / Glyph Trails (five laser effects).
- Original precise FFT spectrum/beat analysis, oscilloscope, metadata popup, settings dialog, original full UI and all themes.
- This version is a **baseline prototype**. Do not use it as a one-to-one performance benchmark of the entire Python application: visual effects and audio analysis are not fully identical yet.
- **Not built or executed here**, because the environment lacks Qt6 development libraries. Build errors or runtime differences may remain.

## Build (Windows 11)

Install Qt **6.8+** (MSVC kit: Widgets, Multimedia, OpenGLWidgets, OpenGL) and CMake, then from a developer command prompt with Qt visible to CMake:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
```

Replace the example Qt path with your installed version. With MSVC, run Qt `windeployqt` on `build/Release/MusiKCpp.exe` for a self-contained app and make sure `shaders/` is next to the executable. FFmpeg/Qt multimedia plugins must be deployed for audio.

## Build (Arch Linux)

```bash
sudo pacman -S cmake ninja gcc qt6-base qt6-multimedia
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/MusiKCpp
```

Requires Qt Multimedia with QAudioBufferOutput (Qt 6.8+). `shaders/` and `MusiK-logo.png` are copied beside the binary automatically.

## Comparison guidance

Use the same monitor/resolution, song, and selected effect. Compare Matrixia vs original Matrixia and GPU shaders vs corresponding original shaders. Capture FPS, CPU and GPU load. Qt vs C++ can improve CPU-heavy glyph creation and audio routines, but GPU fragment shaders are already native on Python, so speedup is not guaranteed there.
