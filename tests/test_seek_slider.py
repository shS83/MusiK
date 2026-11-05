"""A click on the timeline seeks immediately; dragging its handle still works."""

import os
import unittest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import Qt, QPoint
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.seek_slider import SeekSlider


class SeekSliderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_track_click_seeks_and_handle_remains_draggable(self):
        slider = SeekSlider(Qt.Orientation.Horizontal)
        slider.resize(400, 30)
        slider.setRange(0, 10000)
        slider.setValue(0)
        slider.show()
        clicked = []
        released = []
        slider.track_clicked.connect(clicked.append)
        slider.sliderReleased.connect(lambda: released.append(True))
        QTest.mouseClick(slider, Qt.MouseButton.LeftButton, pos=QPoint(300, 15))
        self.assertEqual(len(clicked), 1)
        self.assertEqual(released, [])
        self.assertEqual(slider.value(), clicked[0])
        self.assertGreater(slider.value(), 7000)

        QTest.mouseClick(slider, Qt.MouseButton.LeftButton, pos=QPoint(300, 15))
        self.assertEqual(len(clicked), 1)

        slider.setEnabled(False)
        QTest.mouseClick(slider, Qt.MouseButton.LeftButton, pos=QPoint(100, 15))
        self.assertEqual(len(clicked), 1)
        slider.close()


if __name__ == "__main__":
    unittest.main()
