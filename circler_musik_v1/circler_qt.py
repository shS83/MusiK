"""PySide6 + ModernGL widget for MusiK's existing QOpenGLWidget visualizer API.

In the MusiK package, place circler_qt.py next to circler_reactive.py.
Requires the same OpenGL 3.3 context as the existing MusiK visuals.
"""
from __future__ import annotations

import time

import moderngl
from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtOpenGLWidgets import QOpenGLWidget

from .circler_reactive import CirclerScene, CirclerRenderer


class _Borderless:
    def __init__(self, widget):
        self.widget = widget
        self._normal_geometry = None

    @property
    def active(self):
        return self.widget.isFullScreen()

    @property
    def geometry(self):
        return self._normal_geometry if self.active and self._normal_geometry else self.widget.geometry()

    def set_active(self, active):
        if active and not self.active:
            self._normal_geometry = self.widget.geometry()
            self.widget.showFullScreen()
        elif not active and self.active:
            self.widget.showNormal()
            if self._normal_geometry:
                self.widget.setGeometry(self._normal_geometry)


class CirclerVisualizer(QOpenGLWidget):
    failed = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowFlag(Qt.WindowType.Window, True)
        self.resize(1280, 720)
        self.setWindowTitle("MusiK · Circler Tetra · F11 fullscreen · Esc close")
        self.scene = CirclerScene()
        self.borderless = _Borderless(self)
        self.error = None
        self._ctx = None
        self._renderer = None
        self._clock = time.monotonic()
        self._timer = QTimer(self)
        self._timer.setInterval(16)
        self._timer.timeout.connect(self.update)

    def showEvent(self, event):
        self._clock = time.monotonic()
        self._timer.start()
        super().showEvent(event)

    def hideEvent(self, event):
        self._timer.stop()
        super().hideEvent(event)

    def initializeGL(self):
        try:
            self._ctx = moderngl.create_context(require=330)
            self._renderer = CirclerRenderer(self._ctx)
        except Exception as exc:
            self.error = f"Circler OpenGL initialization failed: {exc}"
            self._timer.stop()
            self.failed.emit(self.error)

    def paintGL(self):
        if self._renderer is None:
            return
        now = time.monotonic()
        dt = min(now - self._clock, .10)
        self._clock = now
        scale = self.devicePixelRatioF()
        w, h = max(1, round(self.width() * scale)), max(1, round(self.height() * scale))
        self.scene.update(dt, w, h)
        try:
            # QOpenGLWidget's framebuffer is NOT FBO 0. Use the Qt-owned FBO.
            target = self._ctx.detect_framebuffer(glo=self.defaultFramebufferObject())
            target.use()
            self._renderer.draw(self.scene, w, h)
        except Exception as exc:
            self._timer.stop()
            self.error = f"Circler rendering failed: {exc}"
            self.failed.emit(self.error)

    def on_beat(self, *_):
        self.scene.beat()

    def on_bands(self, *_):
        # Circler currently reacts only to beats. Retain MusiK signal contract.
        pass

    def on_energy(self, *_):
        pass

    def on_phrase(self, *_):
        pass

    def reset(self):
        self.scene.flash = 0.0
        for tetra in self.scene.tetrahedra:
            tetra.kick = 0.0

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_F11:
            self.borderless.set_active(not self.borderless.active)
            event.accept()
        elif event.key() == Qt.Key.Key_Escape:
            if self.borderless.active:
                self.borderless.set_active(False)
            else:
                self.close()
            event.accept()
        else:
            super().keyPressEvent(event)

    def closeEvent(self, event):
        self._timer.stop()
        if self._renderer is not None:
            self.makeCurrent()
            self._renderer.release()
            self.doneCurrent()
            self._renderer = None
        super().closeEvent(event)
