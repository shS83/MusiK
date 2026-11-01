from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PySide6.QtCore import QSettings, Qt
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow
from musik.audio import BassDetector
from musik.lasers import LaserScene, edge_position
from musik.rhythm import PhraseClock
from musik.settings import DEFAULTS, SettingsDialog, load_settings


class RhythmTests(unittest.TestCase):
    def test_four_and_eight_bar_phrases(self):
        for bars in (4, 8):
            for period in (.4, .5, .75):
                clock = PhraseClock(bars)
                times = []
                dt = period / 100
                for tick in range(4 * bars * 100 * 2 + 2):
                    if clock.advance(dt, onset=tick % 100 == 0):
                        times.append(tick * dt)
                self.assertEqual(len(times), 2, (bars, period, times))
                self.assertAlmostEqual(times[0], period * 4 * bars, delta=dt * 2)
                self.assertAlmostEqual(times[1] - times[0], period * 4 * bars, delta=dt * 2)

    def test_missing_kicks_and_silence(self):
        clock = PhraseClock(4)
        for tick in range(1100):
            clock.advance(.01, onset=tick % 50 == 0 and tick not in (300, 600))
        self.assertAlmostEqual(clock.period, .5, places=5)
        self.assertGreater(clock.phrases, 0)
        for _ in range(1000):
            clock.advance(.01, active=False)
        previous = clock.phrases
        for _ in range(1000):
            clock.advance(.01, active=False)
        self.assertEqual(clock.phrases, previous)
        self.assertIsNone(clock.last_onset)

    def test_decoder_chunk_independence(self):
        rate = 16000
        t = np.arange(rate * 18) / rate
        envelope = np.exp(-((t - .1) % .5) * 30)
        signal = (.65 * np.sin(2 * np.pi * 70 * t) * envelope).astype(np.float32)
        results = []
        for block in (256, 731, 4096):
            detector = BassDetector()
            events = []
            for start in range(0, len(signal), block):
                detector.feed(signal[start:start + block], rate)
                events.extend(detector.phrase_events)
            results.append(events)
        self.assertEqual(results, [[1, 2]] * 3)


class LaserDesktopTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def test_presets_change_and_bound_particles(self):
        for variant in (1,3,4):
            scene = LaserScene(variant)
            before = (scene.rays, scene.per_ray, scene.spin_target, scene.glyph)
            scene.change_phrase(1)
            after = (scene.rays, scene.per_ray, scene.spin_target, scene.glyph)
            if variant == 4:
                self.assertEqual(before, after)
            else:
                self.assertNotEqual(before[:3], after[:3])
                self.assertEqual(before[3], after[3])
            for _ in range(90):
                image = scene.render(480, 300, 1 / 30, .7, .6, .5, .2)
            self.assertFalse(image.isNull())
            self.assertLessEqual(len(scene.particles), 2000)
            scene.reset()
            self.assertEqual(scene.particles, [])
            self.assertIsNone(scene.trails)

    def test_all_modes_with_phrase_signal_and_flash(self):
        with tempfile.TemporaryDirectory() as folder:
            store = QSettings(str(Path(folder) / "settings.ini"), QSettings.Format.IniFormat)
            window = MainWindow(store)
            try:
                window.show()
                window.show_visualizer()
                for effect in range(4, 7):
                    window.apply_settings(dict(DEFAULTS, effect=effect, laser_bars=8))
                    visual = window.visualizer
                    visual.resize(640, 360)
                    for _ in range(12):
                        window.analysis.energy.emit(.8)
                        window.analysis.bands.emit(.5, .65)
                        QTest.qWait(20)
                    self.assertTrue(visual.isValid())
                    self.assertFalse(visual.error, visual.error)
                    before = visual.lasers.changes
                    window.analysis.phrase.emit(1)
                    self.assertEqual(visual.lasers.changes, before)
                    window.set_party_mode(True)
                    window.analysis.phrase.emit(2)
                    self.assertEqual(visual.lasers.changes, before + int(effect != 6))
                    window.set_party_mode(False)
                    palette = visual.lasers.palette_index
                    for _ in range(32):
                        window.analysis.beat.emit(1.0)
                    self.assertNotEqual(palette, visual.lasers.palette_index)
                    visual.reset()
                    self.assertEqual(visual.lasers.particles, [])
                    visual.close()
                    self.assertFalse(visual.timer.isActive())
                    window.show_visualizer()
                    self.assertTrue(visual.timer.isActive())
                self.assertEqual(load_settings(store)["laser_bars"], 8)
                self.assertEqual(load_settings(store)["effect"], 6)
                dialog = SettingsDialog(window.preferences, window)
                self.assertEqual(dialog.values()["laser_bars"], 8)
                dialog.reject()
                window.apply_settings(dict(DEFAULTS, effect=0))
                self.assertEqual(window.analysis.detector.phrase_bars, 4)
            finally:
                window.close()
                window.deleteLater()
                QTest.qWait(20)

    def test_perimeter_wrap(self):
        perimeter = 2 * (800 - 20 + 600 - 20)
        self.assertEqual(edge_position(12, 800, 600), edge_position(perimeter + 12, 800, 600))


if __name__ == "__main__":
    unittest.main()
