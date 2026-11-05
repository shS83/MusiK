# Circler Tetra → MusiK Python (v1)

Source: original `circler.py` from Circler. The 3D tetrahedron mesh, GL 3.3
shaders, 0.3 s spawning interval, rotation, spiral motion, size growth and
screen-exit behavior are preserved. The **color, bass and background** layers
are new.

## Run the standalone demo

From your Circler virtual environment (`uv` available):

```bash
uv sync
uv run python circler_demo.py
```

SPACE or B triggers a test bass hit; A enables/disables a simulated 125 BPM beat;
ESC exits. This demo does **not** capture system audio; live bass hits come from
MusiK's `AudioAnalysis.beat` signal after integration.

## Add to MusiK Python

1. Copy `circler_reactive.py` and `circler_qt.py` into the same Python package
   directory as MusiK's `app.py`.
2. Install ModernGL in MusiK's environment: `uv add moderngl`.
3. In `settings.py`, append `"Circler Tetra"` to the end of the `EFFECTS` tuple.
   Appending is important: it preserves existing saved effect indexes.
4. In `app.py`, add `from .circler_qt import CirclerVisualizer`.
5. In `MainWindow.show_visualizer()` change the widget selection block to:

   ```python
   if effect == len(EFFECTS) - 1:
       self.visualizer = CirclerVisualizer(self)
   elif effect == 0:
       self.visualizer = Visualizer(self)
   else:
       self.visualizer = EffectVisualizer(effect, self)
   ```

6. Leave existing `self.analysis.beat.connect(self.visualizer.on_beat)` and
   `self.analysis.bands.connect(self.visualizer.on_bands)` unchanged.
7. MusiK's C++ and Python builds under Wayland/NVIDIA have previously needed
   `QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_glx`; use this if Qt cannot
   create an OpenGL context.

**Important:** The full current MusiK Python project was not included with the
Circler ZIP. This Qt adapter is ready to plug in, but the final integration
cannot be compiled/tested against MusiK until its `app.py`, `effects.py`,
`visualizer.py`, `audio.py`, and `settings.py` are available. `dispose_visualizer()`
should disconnect its existing `beat`/`bands` signals; Qt auto-disconnects
connections when a QObject is destroyed. Other MusiK APIs may need matching
adjustments based on the actual current source.

## Intended behavior

- Background loops through `(20,20,40)` → `(40,40,20)` → `(40,20,20)` using
  smoothstep interpolation, 12 seconds per stage (36 seconds per loop).
- Initial orange/magenta tetrahedra keep **their birth colors**.
- New generations get randomly chosen neon color pairs every **64 accepted
  beat events** (16 bars assuming 4/4 and one beat event per quarter note).
- The first transient gives all visible tetrahedra a short radial kick and
  a restrained white flash overlay; their regular growth and orbit continue.
- There is a 200 ms beat-event debounce. The counter is an approximation of
  musical bars, not true downbeat/meter detection.

## Testing

```
python -m unittest discover -s tests -v
python -m py_compile circler_reactive.py circler_demo.py circler_qt.py
```

The unit tests avoid OpenGL and can be run with just Python by stubbing the
unavailable renderer module; the actual demo and widget need the listed GPU
libraries and a graphical session.
