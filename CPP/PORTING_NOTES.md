# MusiK C++ experimental parity pass

Built from the supplied Python MusiK and lasers source archives, plus the earlier C++ prototype.

## Included for Windows testing
- Purple/orange styling with now-playing panel and layout closer to Python MusiK
- Folder import (recursive, one folder per dialog), deduplication, playlist M3U8 import/export and session restore
- Audio seek slider, right-click metadata dialog (Qt Multimedia reads selected file asynchronously)
- Oscilloscope from decoded PCM and a 48-band *simple DFT* spectrum approximation (not Python FFT analyzer parity)
- Chrome sphere orbital camera and camera-facing bass kick with 20-ms attack / 180-ms release
- Five distinct laser entries with QPainter glyph cache and trails, including locked v4.3 acute-accent glyph, 36 rays, 150 glyphs/ray, 36-pixel font and 72 rotation steps

## Important differences / limitations
- Laser renderer is a *porting prototype*, not verified frame-for-frame equivalent to Pygame. Real fonts rasterize differently and some laser variants still have approximate animation.
- The v4.3 screenshot's stippled surface may depend on Pygame font antialiasing/rotation. Qt's glyph path rasterizer cannot guarantee identical pixels.
- This code has **not been compiled or run in Windows** here; test as source, not as a validated release.
- Audio spectrum uses a per-band DFT rather than the original 2048-sample FFT and can be CPU-heavy. Folder import is synchronous, unlike original Python background scan; very large folders can freeze UI while scanning.
- Python's 20 theme variants, full settings panel, beat detector, and multiple-folder selection are not yet feature-identical.

## Windows Git Bash build
```
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/6.12.0/mingw_64/bin:$PATH"
cmake -S . -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/mingw_64"
cmake --build build-mingw --parallel
./build-mingw/MusiKCpp.exe
```

Use a fresh `build-mingw` tree for the updated source if stale CMake cache causes trouble.
