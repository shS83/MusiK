# fog_shS – audio-reactive 3D visualizer for MusiK Python

This version adds **fog_shS** as the last entry in Settings → Visualization.

- Preserves the user-provided Fog project's 3D extruded `shS` geometry, slow rotation, mist, blue highlights, and bloom.
- The original mesh is baked into `musik/fog_mesh.npz` (19,068 vertices / 6,356 triangles). The source-compatible generator is `tools/generate_fog_mesh.py`; no mesh dependencies are required at runtime.
- A 512-sample live audio waveform bends the actual letter contours in their local outward normal direction, and slightly in depth. Bass hits temporarily intensify the deformation and glow.
- The audio texture is fixed-size; intermediate render targets are capped at 1920×1080 and reused until a window resize.
- Circler Tetra v3 and earlier visualization IDs are unchanged.

## Run

```bash
uv sync
QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_glx uv run python musik.py
```

In **Settings → Visualization**, select **fog_shS**, then select **Start visualization**. Use F11 for fullscreen, Esc to restore/close.

## Technical notes

- The shader gets current decoded PCM from `AudioAnalysis.waveform`, not generated/synthetic waves.
- On systems where Qt's Wayland/EGL OpenGL context creation fails, run with XWayland (`QT_QPA_PLATFORM=xcb`) as above.
- The runtime does **not** load Matplotlib/Shapely; those tools were used only when baking the mesh from the original project. A build-time mesh generator is provided.
- If GL initialization fails, the window will display an error rather than deliberately queuing frames or audio data.
- Actual Qt/OpenGL rendering and performance need testing on the target computer. The package was checked by Python compilation and mesh/waveform tests, not visually run on a GPU here.
