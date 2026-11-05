# Circler Tetra MusiK Python integration (v3)

The original Circler tetrahedron mesh, its rotating spiral, continuous growth,
spawn rate and offscreen disappearance remain intact. Built directly into the
MusiK Python player; **not** the C++ player.

## Changes

- New **Circler Tetra** entry in Settings → Visualization (index 9, appended
  after Fireworks; no migration of existing saved effect indexes).
- 20% stronger beat-triggered radial hop than v2 (80% stronger than v1); smooth exponential return. The
  existing growth animation continues even while the tetrahedra jump.
- New random neon palette **every 16 accepted bass onsets** (~4 bars in 4/4 if
  each onset represents a quarter-note). Previously 64. Existing tetrahedra
  retain their born colors; only newly spawned objects use the new pairing.
- Initial orange / magenta; background loops blue `(20,20,40)` → yellow
  `(40,40,20)` → red `(40,20,20)` → blue in 12-second transitions.
  A fullscreen radial gradient stays pure black in the center and reaches
  the animated background color only at the window edges, creating a deep
  well/tunnel illusion behind the tetrahedra.
- Short translucent white beat flash. When the player is idle the scene still
  animates, but new colors require beats from actual decoded audio.
- The existing visualizer supports F11/Esc, Party Mode, FPS counter, and
  changing effects while playing. ModernGL was already in MusiK requirements.

## Start

```bash
uv sync
QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_glx uv run musik.py
```

The `xcb` environment variables are only a compatibility workaround for
Qt/NVIDIA/Wayland EGL context failures. Try native Wayland first if it works
on your system.

Settings → Visualization → **Circler Tetra** → Start visualization.

## Tuning

In `musik/circler_reactive.py`:

- `BARS_PER_PALETTE = 4` (with `BEATS_PER_BAR = 4` gives 16 onsets)
- `BASS_KICK_MULTIPLIER = 1.8` (v2 was 1.5)
- `BACKGROUND_SECONDS_PER_STEP = 12.0`

Bass onset detection is heuristic. This is not true time-signature or
measure/downbeat detection, so manual presses and music may differ.

## Test

```bash
python -m unittest discover -s tests -p 'test_circler.py' -v
python -m compileall -q musik
# Optional standalone keyboard demo (Pygame required):
uv run --with pygame python circler_demo.py
```

OpenGL rendering needs Qt/PySide6, ModernGL and a desktop GL 3.3 context; tests
in `test_circler.py` validate the numerical scene behavior without a display.
