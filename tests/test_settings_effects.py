from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PySide6.QtCore import QItemSelectionModel, QSettings, Qt
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow
from musik.audio import AudioAnalysis
from musik.effects import MusicMotion
from musik.folders import FolderDialog
from musik.library import scan_audio_folders
from musik.settings import DEFAULTS, SettingsDialog, THEMES, load_settings, make_themes


class SettingsTests(unittest.TestCase):
    def test_fixed_themes_and_invalid_settings(self):
        self.assertEqual(THEMES, make_themes())
        self.assertEqual(len(THEMES), 20)
        self.assertEqual(len(set(colors for _, colors in THEMES)), 20)
        with tempfile.TemporaryDirectory() as folder:
            store = QSettings(str(Path(folder) / "settings.ini"), QSettings.Format.IniFormat)
            store.setValue("theme", 999)
            store.setValue("spectrum_bands", "broken")
            values = load_settings(store)
            self.assertEqual(values["theme"], 19)
            self.assertEqual(values["spectrum_bands"], 48)

    def test_overlapping_folders_and_cancel(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            first, second = root / "one", root / "two"
            first.mkdir()
            second.mkdir()
            for path in (first / "a.wav", second / "b.FLAC"):
                path.touch()
            results = list(scan_audio_folders([first, second, root]))
            self.assertEqual(len(results), 2)
            self.assertEqual(len(set(results)), 2)
            self.assertEqual(list(scan_audio_folders([root], cancelled=lambda: True)), [])

    def test_spectrum_reconfiguration_and_reset_signal(self):
        analysis = AudioAnalysis()
        received = []
        analysis.spectrum.connect(lambda values, maximum: received.append(values))
        for count in (16, 48, 128):
            analysis.set_spectrum_bands(count)
            tone = .2 * np.sin(2 * np.pi * 1000 * np.arange(24000) / 48000)
            analysis.detector.feed(tone, 48000)
            self.assertEqual(len(analysis.detector.spectrum_levels), count)
            self.assertGreater(max(analysis.detector.spectrum_levels), .7)
            analysis.reset()
            self.assertEqual(len(received[-1]), count)
            self.assertEqual(max(received[-1]), 0)

    def test_music_drives_speed_and_quiet_transition_reverses(self):
        motion = MusicMotion()
        for _ in range(180):
            motion.step(1 / 60, .9, .7, .6, .2)
        loud_speed = motion.speed
        self.assertGreater(loud_speed, 4)
        reversed_motion = False
        for _ in range(240):
            motion.step(1 / 60, .02, .01, .01, 0)
            reversed_motion |= motion.speed < 0
        self.assertTrue(reversed_motion)
        self.assertLess(abs(motion.speed), loud_speed / 4)


class DesktopSettingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = Path(self.folder.name)
        self.store = QSettings(str(self.root / "settings.ini"), QSettings.Format.IniFormat)
        self.window = MainWindow(self.store)
        self.window.output.setMuted(True)

    def tearDown(self):
        self.window.close()
        self.window.deleteLater()
        QTest.qWait(20)
        self.folder.cleanup()

    def test_settings_apply_reload_sort_and_multifile(self):
        paths = [self.root / name for name in ("z.wav", "a.wav", "m.wav")]
        for path in paths:
            path.touch()
        with patch("musik.app.QFileDialog.getOpenFileNames", return_value=([str(p) for p in paths], "")):
            self.window.add_files()
        self.assertEqual(self.window.playlist.count(), 3)
        current = self.window.playlist.item(0)
        self.window.current_item = current
        values = dict(DEFAULTS, order=1, theme=7, spectrum_bands=96, scope_points=1024)
        self.window.apply_settings(values)
        self.assertEqual([self.window.playlist.item(i).text() for i in range(3)], ["a", "m", "z"])
        self.assertIs(self.window.current_item, current)
        self.assertEqual(load_settings(QSettings(str(self.root / "settings.ini"), QSettings.Format.IniFormat)), values)
        self.assertEqual(self.window.scope.scope_points, 1024)
        self.assertEqual(len(self.window.scope.spectrum), 96)
        dialog = SettingsDialog(values, self.window)
        self.assertEqual(dialog.values(), values)
        dialog.reject()
        self.window.apply_settings(dict(values, order=0))
        self.assertEqual([self.window.playlist.item(i).text() for i in range(3)], ["z", "a", "m"])
        self.window.apply_settings(dict(values, order=2))
        self.assertEqual([self.window.playlist.item(i).text() for i in range(3)], ["z", "m", "a"])

    def test_folder_dialog_selects_multiple_rows(self):
        paths = [self.root / name for name in ("album-a", "album-b")]
        for path in paths:
            path.mkdir()
        dialog = FolderDialog(self.window)
        dialog.show()
        for path in paths:
            index = dialog.model.index(str(path))
            self.assertTrue(index.isValid())
            dialog.tree.selectionModel().select(index, QItemSelectionModel.SelectionFlag.Select | QItemSelectionModel.SelectionFlag.Rows)
        dialog.accept()
        self.assertEqual(set(map(Path, dialog.folders())), set(paths))

    def test_effects_render_switch_resize_reopen_and_matrix_click(self):
        self.window.show()
        self.window.show_visualizer()
        for effect in (1, 2, 3, 0, 2):
            self.window.apply_settings(dict(DEFAULTS, effect=effect))
            visual = self.window.visualizer
            visual.resize(640, 360)
            visual.on_bands(.7, .65)
            visual.on_beat(.8)
            if effect:
                visual.on_energy(.8)
            QTest.qWait(160)
            if effect == 3:
                # Streams now enter from above and leave stationary glyphs.
                # Keep audio arriving while enough rows enter the viewport.
                for _ in range(20):
                    visual.on_energy(.8)
                    visual.on_bands(.7, .65)
                    QTest.qWait(30)
            self.assertFalse(visual.error, visual.error)
            self.assertTrue(visual.isValid())
            frame = visual.grabFramebuffer()
            self.assertFalse(frame.isNull())
            colors = {frame.pixelColor(x, y).name() for x in range(0, frame.width(), 30)
                      for y in range(0, frame.height(), 30)}
            self.assertGreater(len(colors), 5)
            if effect == 3:
                previous = visual.matrix_color
                QTest.mouseClick(visual, Qt.MouseButton.LeftButton)
                self.assertNotEqual(visual.matrix_color, previous)
            visual.reset()
            self.assertEqual(visual.motion, [0, 0])
            visual.close()
            self.assertFalse(visual.timer.isActive())
            self.window.show_visualizer()
            self.assertTrue(visual.timer.isActive())


if __name__ == "__main__":
    unittest.main()
