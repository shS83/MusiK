"""Regression checks for Python playback settings and the two ported GPU scenes."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

import numpy as np
from PySide6.QtCore import QSettings
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.app import MainWindow
from musik.effects import EffectVisualizer
from musik.fireworks import Rocket
from musik.settings import EFFECTS, load_settings


class PortTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def test_legacy_effect_selections(self):
        with tempfile.TemporaryDirectory() as folder:
            store = QSettings(str(Path(folder)/"prefs.ini"), QSettings.IniFormat)
            for old, expected in ((4,"Chrome sphere"),(5,"Orbit Streams"),(6,"Chrome sphere"),(7,"Glyph Spin"),(8,"Glyph Trails")):
                store.setValue("effect", old)
                self.assertEqual(EFFECTS[load_settings(store)["effect"]],expected)
            store.setValue("effectListVersion",2)
            self.assertEqual(EFFECTS[load_settings(store)["effect"]],"Fireworks")

    def test_render_ported_scenes(self):
        for effect in (7,8):
            visual = EffectVisualizer(effect)
            try:
                visual.resize(400,300)
                visual.show()
                QTest.qWait(100)
                self.assertTrue(visual.isValid())
                self.assertFalse(visual.error,visual.error)
                visual.timer.stop()
                if effect==8:
                    fire = visual.fire
                    fire.rng = np.random.default_rng(42)
                    # Advance a whole launch, its explosion, and delayed crackles.
                    visual.makeCurrent()
                    visual.ctx.detect_framebuffer(visual.defaultFramebufferObject()).use()
                    peak = 0
                    for _ in range(150):
                        fire.draw(.04,400,300,1,.8)
                        peak = max(peak,len(fire.sparks))
                    self.assertGreater(peak,1300)
                    visual.doneCurrent()
                image = visual.grabFramebuffer()
                self.assertFalse(image.isNull())
                pixels = np.frombuffer(image.constBits(),np.uint8).reshape(image.height(),-1)
                self.assertGreater(np.unique(pixels).size,20)
            finally:
                visual.close()
                visual.deleteLater()
                QTest.qWait(10)

    def test_firework_patterns_and_tip_fade(self):
        visual = EffectVisualizer(8)
        try:
            visual.resize(400, 300)
            visual.show()
            QTest.qWait(100)
            visual.timer.stop()
            fire = visual.fire
            fire.rng = np.random.default_rng(17)
            rocket = Rocket(np.array([200., 150.]), np.zeros(2), np.ones(3), 190, 0, 0, False)
            for pattern in ("oval", "double ring", "flower", "comets"):
                fire.reset()
                fire.pattern_burst(rocket, .8, .7, pattern)
                visual.makeCurrent()
                target = visual.ctx.detect_framebuffer(visual.defaultFramebufferObject())
                target.use()
                for _ in range(12):
                    fire.draw(.04, 400, 300, 1, .7)
                pixels = np.frombuffer(target.read(components=3), dtype=np.uint8)
                self.assertGreater(np.count_nonzero(pixels > 50), 100, pattern)
                visual.doneCurrent()
            fire.time = 1.5
            full = fire.comet_points(1)[0]
            fire.time = 2.6
            fading = fire.comet_points(1)[0]
            self.assertGreater(len(full), len(fading))
            np.testing.assert_array_equal(fading[0, :2], rocket.pos)
            fire.time = 3.51
            self.assertFalse(fire.comet_points(1))

            fire.cloud_burst = Mock()
            fire.pattern_burst = Mock()
            fire.queue_crackles = Mock()
            joint = 0
            for _ in range(2000):
                before = fire.pattern_burst.call_count, fire.queue_crackles.call_count
                fire.burst(rocket, 1, .5)
                joint += int(fire.pattern_burst.call_count > before[0] and fire.queue_crackles.call_count > before[1])
            self.assertAlmostEqual(fire.pattern_burst.call_count/2000, .20, delta=.03)
            self.assertAlmostEqual(fire.queue_crackles.call_count/2000, .25, delta=.03)
            self.assertAlmostEqual(joint/2000, .05, delta=.02)
        finally:
            visual.close()
            visual.deleteLater()
            QTest.qWait(10)

    def test_brand_and_playback(self):
        with tempfile.TemporaryDirectory() as folder:
            store = QSettings(str(Path(folder)/"prefs.ini"), QSettings.IniFormat)
            w = MainWindow(store)
            try:
                self.assertEqual(w.brand.text(),"μSIK")
                self.assertEqual(w.windowTitle(),"μSIK")
                files = []
                for album in ("A","B","C"):
                    d = Path(folder)/album
                    d.mkdir()
                    for track in ("01","02"):
                        file = d/(track+".wav")
                        file.touch()
                        files.append(file)
                w.add_paths(files+files)
                self.assertEqual(w.playlist.count(),6)
                # Exercise Next without starting a decoder for the empty fixtures.
                w.play_item = lambda item: setattr(w, "current_item", item)
                for mode in (1,2,3):
                    w.current_item = w.playlist.item(0) if mode==1 else None
                    w.apply_settings(dict(w.preferences, playback=mode))
                    order = []
                    for _ in files:
                        w.next()
                        order.append(w.playlist.row(w.current_item))
                    self.assertEqual(sorted(order),list(range(6)))
                    if mode==1:
                        self.assertEqual(order,[5,4,3,2,1,0])
                    if mode==3:
                        for i in (0,2,4):
                            self.assertEqual(files[order[i]].parent,files[order[i+1]].parent)
            finally:
                w.close()

