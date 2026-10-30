# MusiK C++: user-feedback patch (experimental)

Changes implemented in this patch:
- 156 Hz-oriented redraw scheduling (`Qt::PreciseTimer`, 6 ms); actual FPS depends on v-sync, platform and CPU/GPU, and is **not** guaranteed to equal 156.
- FFT replaces the 48 separate O(N) frequency-domain summations for scope spectrum; remains a lightweight approximate 1024-point spectrum, not full Python audio analysis parity.
- Glyph Spin: 15 rays x 180 'O' characters; slight audio-reactive deviations; original glyph always retained.
- Glyph Waves: '?' rotates continuously counterclockwise instead of oscillating, with palette cycling.
- Palette cycles use 16-color reference palette, beat counter (16 beats / four bars by default) plus 8 s fallback. Four bars assumes 4/4 time; beat-detection is basic and imperfect.
- Glyph Trails v4.3 keeps the literal '´ ' glyph, 36 rays, 150 instances/ray and 36px font. Trailing image is recolored on palette changes.
- Party Mode button (enables laser parameter variation but **does not modify glyphs**); initial settings dialog controls 2/4/8 bars between palette changes.
- Original purple-orange diagonal background gradient, roomier margins, playing row indicator and 1-based playlist numbering.
- Stronger Hiss refraction and color-channel split inputs.
- Fixes coordinate narrowing warnings in `edgePosition`.

Known gaps / caveats:
- Windows Qt6/MinGW build was not executable/testable in this environment; compile and check there.
- Scope FPS cannot exceed actual decoded audio sample buffer arrival rate unless interpolation is added. Fast Qt redraw timer does not automatically update PCM data.
- The original Python settings dialog has far more preferences than this initial C++ settings dialog.
- Orbit Streams still differs structurally from original Python algorithm; matching the screenshot requires further source-accurate porting.
- Hiss temporal trails have not yet been implemented (only the existing spatial GLSL refractive shader parameters were intensified).
- Full 1:1 laser shader/font parity not yet verified against Pygame: pixel rasterization may differ.
