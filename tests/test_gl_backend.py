from pathlib import Path
import tempfile
import unittest

from PySide6.QtCore import QSettings

from musik.gl_backend import apply_backend, available_backends
from musik.settings import load_settings


class GlBackendTests(unittest.TestCase):
    def test_only_installed_integrations_for_active_displays_are_listed(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for relative in ("platforms/libqwayland.so", "platforms/libqxcb.so",
                             "xcbglintegrations/libqxcb-glx-integration.so",
                             "xcbglintegrations/libqxcb-egl-integration.so"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            keys = [key for key, _ in available_backends(root, {"DISPLAY": ":0", "WAYLAND_DISPLAY": "wayland-0"})]
            self.assertIn("wayland_egl", keys)
            self.assertIn("x11_glx", keys)
            self.assertIn("x11_egl", keys)
            self.assertEqual([key for key, _ in available_backends(root, {})], ["auto"])

    def test_backend_environment_is_set_before_qt_starts(self):
        environment = {"QT_QPA_PLATFORM": "wayland", "QT_XCB_GL_INTEGRATION": "xcb_egl"}
        apply_backend("x11_glx", environment)
        self.assertEqual(environment["QT_QPA_PLATFORM"], "xcb")
        self.assertEqual(environment["QT_XCB_GL_INTEGRATION"], "xcb_glx")
        apply_backend("software_glx", environment)
        self.assertEqual(environment["LIBGL_ALWAYS_SOFTWARE"], "1")
        apply_backend("wayland_egl", environment)
        self.assertEqual(environment["QT_QPA_PLATFORM"], "wayland")
        self.assertNotIn("QT_XCB_GL_INTEGRATION", environment)
        self.assertNotIn("LIBGL_ALWAYS_SOFTWARE", environment)
        apply_backend("auto", environment)
        self.assertEqual(environment["QT_QPA_PLATFORM"], "wayland")

    def test_invalid_saved_choice_falls_back_to_auto(self):
        with tempfile.TemporaryDirectory() as folder:
            store = QSettings(str(Path(folder) / "settings.ini"), QSettings.Format.IniFormat)
            store.setValue("gl_backend", "unknown")
            self.assertEqual(load_settings(store)["gl_backend"], "auto")
            store.setValue("gl_backend", "x11_glx")
            self.assertEqual(load_settings(store)["gl_backend"], "x11_glx")
