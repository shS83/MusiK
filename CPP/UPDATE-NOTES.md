# MusiK C++ — borderless / laser / UI test patch

This is an experimental source patch intended for Windows MinGW + Qt6 testing. It has **not** been compiled or run against Qt in this environment.

## Implemented

- F11 toggles a frameless desktop-sized normal window (not `showFullScreen()`), restoring previous geometry/flags upon second press. Escape restores the window first, then closes it.
- Laser Nexus and Orbit Streams are redrawn as densely overlapping antialiased filled ribbon paths (rotating with the effect), rather than sparse glyphs, aiming to eliminate black speckles and holes.
- Glyph Waves removed from the visualization choices. Glyph Spin retains the original O glyph and 15×180 layout; Glyph Trails retains the original acute-accent glyph.
- Party Mode now opens the visualization, rotates through effects, and cycles the MusiK heading's rainbow colors.
- Expanded settings: visualization, palette interval, sort, playback order, scope points, spectrum bars, and predefined/custom gradients.
- Compact symbol-only transport controls in the top bar; mute by the volume slider; track numbering and play indicator; recursive folder import and autosave retained.
- Removed fullscreen white flash overlay over laser surface detail.

## Known limitations and tests

- `Shuffle (albums)` currently groups by parent directory as a heuristic, **not** by album metadata. Random and shuffle paths require real-world verification.
- Spectrum display can draw roughly one bar per pixel, but the audio analysis still has 48 FFT-derived frequency bands; higher bar counts interpolate/repeat those values. This is *not* a higher-resolution FFT implementation.
- Party Mode changes effects at a fixed 12-second interval, not musical bars; real BPM-aware sequencing remains undone.
- Original Python settings are **not fully ported** yet, and GPU/CPU load at 4K is unverified.
- Test F11 with your television's HDR/SDR color presentation; borderless does not guarantee color correctness.
- Build and inspect actual compile errors in Windows; no Qt6 toolchain was available for this build here.
