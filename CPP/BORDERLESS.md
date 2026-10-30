# Borderless presentation on Windows

F11 changes the existing HWND's frame and monitor-sized rectangle. ESC restores
the saved WINDOWPLACEMENT; another ESC closes the visualization. The OpenGL
context is kept alive. No display-mode change API is used.

A driver may promote a fully opaque, screen-sized OpenGL window to fullscreen
presentation even when the application never requests exclusive fullscreen.
To discourage that path, borderless mode uses WS_EX_LAYERED and alpha 254/255
before enlarging the window. This introduces approximately 0.4% desktop blending
and can affect composition cost. The original extended style is restored only
after restoring the window size. Python uses the same native technique, including
when Party Mode or settings replaces the visualizer.

The regression checks the window handle, OpenGL context, monitor coverage,
blending flag, window-style/geometry restoration (including maximized windows),
and unchanged reported display mode. These checks cannot detect a physical HDMI
link retrain, HDR/VRR transition or TV blanking; verify repeated F11 transitions
on the affected TV. Qt may infer isFullScreen() from native window geometry, so
that property is not evidence of an exclusive display-mode request.

Build with -DMUSIK_BUILD_TESTS=ON and run ctest --output-on-failure in the build
directory with Qt and compiler runtime DLLs on PATH. Python regression:
python -m unittest tests.test_borderless tests.test_metadata_party

References:
- https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features
- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model
