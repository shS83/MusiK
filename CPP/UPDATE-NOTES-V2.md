# MusiK C++ — display/spectrum fix test

Changes in src/main.cpp:
- Transport buttons now use explicit white Unicode glyphs in the correct Previous / Play / Stop / Next order.
- Mute/visualization/party symbols have explicit white foreground colors.
- Scope starts fully stretched. Left click cycles Oscilloscope, Spectrum and split Combo mode. A setting is provided to select the mode.
- Spectrum now uses the original FFT's individual 2048 bins from a 4096-point FFT instead of just repainting 48 aggregated frequency bands. Rendering groups FFT bins only if fewer than 2048 bars are selected. Frequency scale is linear (not a logarithmic frequency-axis redesign).
- Background presets include black-to-white and five bright/dark contrasting gradients; custom stops remain.
- F11 uses a desktop-sized frameless normal window and reapplies its geometry after Qt processes window flags, without calling showFullScreen. Windows/HDR behavior is NOT guaranteed, and must be checked on your display.

Known limitations:
- Not compiled or runtime-tested with Qt6 on Windows in this environment.
- FFT can cost more CPU than the preceding 1024-bin version. Test at 156/165 Hz and assess CPU load. FFT updates follow QAudioBuffer callbacks rather than monitor refresh.
- Fast borderless toggling may still be handled differently by Windows compositor and GPU driver. No exclusive display mode is requested.
- Full Windows color/HDR handling not implemented. If the television still dims or changes saturation, further instrumentation of the swap chain / Windows HDR state is needed.
