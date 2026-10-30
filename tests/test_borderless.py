import ctypes as c
from ctypes import wintypes as t
import sys
import unittest

from PySide6.QtCore import Qt
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication
from musik.visualizer import Visualizer


@unittest.skipUnless(sys.platform == "win32", "Native Windows regression")
class BorderlessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def test_repeated_f11_preserves_context_styles_and_geometry(self):
        u = c.WinDLL("user32")
        u.GetWindowLongPtrW.argtypes = [t.HWND, c.c_int]
        u.GetWindowLongPtrW.restype = c.c_ssize_t
        u.GetWindowRect.argtypes = [t.HWND, c.POINTER(t.RECT)]
        def rect(hwnd):
            r = t.RECT()
            self.assertTrue(u.GetWindowRect(hwnd, c.byref(r)))
            return r.left, r.top, r.right, r.bottom
        w = Visualizer()
        try:
            w.resize(640, 360)
            w.show()
            QTest.qWait(150)
            hwnd, context = int(w.winId()), w.context()
            styles = [u.GetWindowLongPtrW(hwnd, i) for i in (-16, -20)]
            original = rect(hwnd)
            for _ in range(3):
                QTest.keyClick(w, Qt.Key_F11)
                QTest.qWait(60)
                self.assertTrue(w.borderless.active)
                # Qt may infer WindowFullScreen from native monitor-sized geometry.
                # Verify the actual HWND/styles instead of that derived Qt flag.
                self.assertEqual(int(w.winId()), hwnd)
                self.assertEqual(w.context(), context)
                self.assertFalse(u.GetWindowLongPtrW(hwnd, -16) & 0x00C40000)
                self.assertTrue(u.GetWindowLongPtrW(hwnd, -20) & 0x80000)
                self.assertFalse(w.grabFramebuffer().isNull())
                QTest.keyClick(w, Qt.Key_Escape)
                QTest.qWait(60)
                self.assertFalse(w.borderless.active)
                self.assertTrue(w.isVisible())
                self.assertEqual(rect(hwnd), original)
                self.assertEqual([u.GetWindowLongPtrW(hwnd, i) for i in (-16, -20)], styles)
            w.showMaximized()
            QTest.qWait(60)
            maximized = rect(hwnd)
            QTest.keyClick(w, Qt.Key_F11)
            QTest.qWait(60)
            QTest.keyClick(w, Qt.Key_F11)
            QTest.qWait(60)
            self.assertEqual(rect(hwnd), maximized)
            QTest.keyClick(w, Qt.Key_Escape)
            self.assertFalse(w.isVisible())
        finally:
            w.close()


if __name__ == "__main__":
    unittest.main()
