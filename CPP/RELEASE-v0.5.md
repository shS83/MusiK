# MusiK Fireworks v0.5

- Fuzzy subsequence playlist search under file buttons; Enter plays first match, Esc clears. Existing playlist items are hidden (not deleted).
- Black > prompt with white shadow.
- Fireworks draws spark/rocket sprites with an OpenGL 3.3 shader, using a soft continuous radial falloff rather than concentric halo discs.
- Rocket explosions retain a single primary color (crackle bursts can be white/gold).
- Burst distribution rebalanced: much less dense at the center, broad mid-shell density.
- **GPU handles rasterization only**; simulation, spawning, crackle and movement remain CPU-side.
- F11 debug mode logging remains.

Build with the existing CMake project on Windows Qt 6.12 + MinGW. Not compiled here because Qt6 development headers/libraries are unavailable.
