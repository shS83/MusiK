"""AttractorPack dynamics, 15-second switching and player/Qt integration."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PySide6.QtCore import Qt, QSettings, QCoreApplication, QEvent
from PySide6.QtGui import QSurfaceFormat
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from musik.attractor_pack import AttractorPackScene, LIMITS, NAMES
from musik.attractor_pack_qt import AttractorPackVisualizer
from musik.settings import EFFECTS, load_settings


class AttractorPackSceneTests(unittest.TestCase):
    def test_timer_defaults_toggle_wrap_and_manual_selection(self):
        scene = AttractorPackScene(100)
        self.assertFalse(scene.hud_visible)
        self.assertTrue(scene.auto_switch)
        self.assertEqual(scene.name, 'Clifford')
        scene.advance_clock(14.99)
        self.assertEqual(scene.current, 0)
        scene.advance_clock(.02)
        self.assertEqual(scene.current, 1)
        self.assertAlmostEqual(scene.switch_elapsed, .01)
        scene.advance_clock(45)
        self.assertEqual(scene.current, 0)
        scene.toggle_auto()
        scene.advance_clock(100)
        self.assertEqual(scene.current, 0)
        self.assertEqual(scene.switch_elapsed, 0)
        scene.toggle_auto()
        scene.advance_clock(14)
        scene.select(3)
        self.assertEqual(scene.switch_elapsed, 0)
        scene.advance_clock(14.99)
        self.assertEqual(scene.current, 3)
        scene.advance_clock(.02)
        self.assertEqual(scene.current, 0)
        # Wall time must be independent of the simulation's 50 ms clamp/drift pause.
        scene.paused = True
        scene.render(15.1, 40, 30)
        self.assertEqual(scene.current, 1)
        self.assertEqual(scene.time, 0)

    def test_all_maps_render_and_react_to_music(self):
        images = []
        for index, name in enumerate(NAMES):
            with self.subTest(attractor=name):
                quiet, loud = AttractorPackScene(6000), AttractorPackScene(6000)
                quiet.select(index)
                loud.select(index)
                loud.beat()
                for _ in range(90):
                    a = quiet.render(1/60, 240, 180)
                    b = loud.render(1/60, 240, 180, 1, .8, .7)
                self.assertGreater(np.count_nonzero(a), 500)
                self.assertGreater(np.mean(np.abs(a.astype(float)-b)), 1)
                self.assertGreater(loud.phase, quiet.phase)
                for points in (loud.x, loud.y, loud.z):
                    self.assertTrue(np.isfinite(points).all())
                    self.assertLess(np.abs(points).max(), LIMITS[index])
                images.append(a)
                for _ in range(80):
                    loud.render(.05, 80, 60)
                self.assertLess(loud.levels.max(), 1e-5)
                self.assertLess(loud.bass, 1e-5)
        for a, b in zip(images, images[1:]):
            self.assertGreater(np.count_nonzero(a != b), 500)

    def test_audio_off_burst_recovery_and_resize(self):
        for index in range(4):
            a, b = AttractorPackScene(1000), AttractorPackScene(1000)
            a.select(index)
            b.select(index)
            a.toggle_audio()
            b.toggle_audio()
            a.beat()
            np.testing.assert_array_equal(a.render(.02, 200, 140, 1, 1, 1), b.render(.02, 200, 140))
            before = a.x.copy()
            a.burst()
            self.assertGreater(np.count_nonzero(a.x != before), 100)
            # Escape/NaN protection is essential for the two Euler-integrated flows.
            a.x[0], a.y[1], a.z[2] = np.nan, np.inf, 1e10
            a.render(.02, 200, 140)
            self.assertTrue(all(np.isfinite(v).all() for v in (a.x, a.y, a.z)))
            for palette in range(4):
                a.palette = palette
                image = a.render(.02, 4000, 2000)
                self.assertEqual(image.shape, (550, 1100, 3))
                self.assertGreater(image.max(), 50)
            a.reset()
            self.assertEqual(a.hist.max(), 0)
            self.assertFalse(a.audio_enabled)
            self.assertEqual(a.render(.02, 1, 1).shape, (1, 1, 3))


class AttractorPackQtTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        cls.app = QApplication.instance() or QApplication([])

    def test_gpu_equations_reduction_and_context_recreation(self):
        from musik.attractor_pack_renderer import AttractorPackRenderer
        visual = AttractorPackVisualizer()
        renderer = None
        try:
            visual.resize(320, 240)
            visual.show()
            QTest.qWait(80)
            visual.timer.stop()
            self.assertFalse(visual.error, visual.error)
            self.assertTrue(visual.scene.gpu)
            self.assertFalse(hasattr(visual.scene, 'x'), 'GPU widget allocated CPU particles')
            self.assertFalse(hasattr(visual.scene, 'hist'), 'GPU widget allocated CPU histogram')
            visual.makeCurrent()
            renderer = AttractorPackRenderer(visual.ctx, 256)
            for mode in range(4):
                # Compare one actual GPU integration step to the original CPU equations,
                # disabling random respawns to make the numerical comparison meaningful.
                reference = AttractorPackScene(256)
                reference.select(mode)
                points = np.column_stack((reference.x, reference.y, reference.z, np.ones(256))).astype('f4')
                renderer.particles[0][0].write(points.tobytes())
                parameters = tuple(reference.parameters)+(0.,)*(6-len(reference.parameters))
                visual.ctx.disable(__import__('moderngl').BLEND)
                renderer._pass('update', renderer.particles[1][1], {
                    'kind': mode, 'parameters': parameters[:4], 'extra': parameters[4:],
                    'drive': 1.8, 'rebirth': 0., 'initialize_points': 0, 'burst': 0,
                    'integrate': 1, 'count': 256, 'serial': 1,
                }, (('particles', renderer.particles[0][0]),))
                actual = np.frombuffer(renderer.particles[1][0].read(), np.float32).reshape(-1,4)
                reference.step(1.8)
                np.testing.assert_allclose(actual[:,:3], np.column_stack((reference.x, reference.y, reference.z)),
                                           atol=4e-5, rtol=4e-5)
            # Odd-sized reduction must include the final row/column, including a bright corner.
            renderer._resize(33, 19)
            density = np.zeros((19,33), np.float32)
            density[-1,-1] = 1234
            renderer.densities[0][0].write(density.tobytes())
            source = renderer.densities[0][0]
            for texture, fbo in renderer.reductions:
                renderer._pass('reduce', fbo, {}, (('source', source),))
                source = texture
            self.assertEqual(np.frombuffer(source.read(), np.float32)[0], 1234)
            self.assertEqual(visual.ctx.error, 'GL_NO_ERROR')
            renderer.release()
            renderer = None
            visual.doneCurrent()
            # Qt may replace the GL context when entering/leaving borderless mode.
            visual.cleanup()
            visual.makeCurrent()
            visual.initializeGL()
            visual.doneCurrent()
            for size in ((1,1), (361,241), (640,360)):
                visual.resize(*size)
                QTest.qWait(20)
                frame = visual.grabFramebuffer()
                self.assertFalse(frame.isNull())
                self.assertFalse(visual.error, visual.error)
        finally:
            if renderer is not None:
                visual.makeCurrent()
                renderer.release()
            visual.close()
            visual.deleteLater()
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)

    def test_player_controls_switch_resize_and_reopen(self):
        from musik.app import MainWindow
        with tempfile.TemporaryDirectory() as folder, patch.object(MainWindow, 'autosave_playlist'):
            store = QSettings(str(Path(folder)/'prefs.ini'), QSettings.Format.IniFormat)
            store.setValue('effectListVersion', 2)
            # Replaces the previous Clifford slot, preserving saved selections.
            store.setValue('effect', 12)
            self.assertEqual(EFFECTS[load_settings(store)['effect']], 'AttractorPack')
            self.assertNotIn('Clifford', EFFECTS)
            window = MainWindow(store)
            try:
                window.show_visualizer()
                visual = window.visualizer
                self.assertIsInstance(visual, AttractorPackVisualizer)
                self.assertFalse(visual.scene.hud_visible)
                self.assertTrue(visual.scene.auto_switch)
                visual.resize(640, 360)
                window.analysis.energy.emit(.9)
                window.analysis.bands.emit(.8, .7)
                window.analysis.beat.emit(1.)
                QTest.qWait(150)
                self.assertGreater(visual.scene.levels.max(), .1)
                self.assertTrue(visual.isValid())
                for key, index in ((Qt.Key.Key_1, 0), (Qt.Key.Key_2, 1), (Qt.Key.Key_3, 2), (Qt.Key.Key_4, 3)):
                    QTest.keyClick(visual, key)
                    self.assertEqual(visual.scene.current, index)
                    QTest.qWait(120)
                    frame = visual.grabFramebuffer()
                    pixels = np.frombuffer(frame.constBits(), dtype=np.uint8)
                    self.assertGreater(np.unique(pixels).size, 50)
                    frame.save(str(Path(tempfile.gettempdir())/f'musik-attractor-pack-python-{index}.png'))
                QTest.keyClick(visual, Qt.Key.Key_N)
                self.assertEqual(visual.scene.current, 0)
                QTest.keyClick(visual, Qt.Key.Key_T)
                self.assertFalse(visual.scene.auto_switch)
                visual.scene.advance_clock(30)
                self.assertEqual(visual.scene.current, 0)
                QTest.keyClick(visual, Qt.Key.Key_T)
                visual.scene.advance_clock(15)
                self.assertEqual(visual.scene.current, 1)
                for expected in (True, False):
                    QTest.keyClick(visual, Qt.Key.Key_H)
                    self.assertEqual(visual.scene.hud_visible, expected)
                QTest.keyClick(visual, Qt.Key.Key_A)
                self.assertFalse(visual.scene.audio_enabled)
                window.analysis.beat.emit(1.)
                self.assertEqual(visual.scene.bass, 0)
                QTest.keyClick(visual, Qt.Key.Key_C)
                self.assertEqual(visual.scene.palette, 1)
                QTest.keyClick(visual, Qt.Key.Key_Space)
                self.assertTrue(visual.scene.paused)
                QTest.keyClick(visual, Qt.Key.Key_R)
                QTest.mouseClick(visual, Qt.MouseButton.LeftButton)
                # Screenshot is a clean image without the optional HUD.
                with patch('musik.attractor_pack_qt.Path.cwd', return_value=Path(folder)):
                    QTest.keyClick(visual, Qt.Key.Key_S)
                    visual.grabFramebuffer()
                    self.assertEqual(len(list(Path(folder).glob('attractorpack_*.png'))), 1)
                visual.resize(360, 640)
                QTest.qWait(40)
                self.assertEqual(visual.grabFramebuffer().size(), visual.size()*visual.devicePixelRatioF())
                visual.reset()
                self.assertEqual(visual.energy, 0)
                visual.close()
                self.assertFalse(visual.timer.isActive())
                index, elapsed = visual.scene.current, visual.scene.switch_elapsed
                visual.last_frame -= 100
                window.show_visualizer()
                self.assertTrue(visual.timer.isActive())
                visual.grabFramebuffer()
                self.assertEqual(visual.scene.current, index)
                self.assertLess(visual.scene.switch_elapsed-elapsed, 1)
                for effect in ('Crawler', 'AttractorPack', 'Chrome sphere', 'AttractorPack'):
                    window.apply_settings(dict(window.preferences, effect=EFFECTS.index(effect)))
                    window.analysis.energy.emit(.5)
                    QTest.qWait(30)
                    self.assertFalse(window.visualizer.error, window.visualizer.error)
                self.assertIsInstance(window.visualizer, AttractorPackVisualizer)
                self.assertFalse(window.visualizer.scene.hud_visible)
                self.assertTrue(window.visualizer.scene.auto_switch)
            finally:
                if window.visualizer is not None:
                    window.dispose_visualizer()
                window.close()
                window.deleteLater()
                QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
                self.app.processEvents()


if __name__ == '__main__':
    unittest.main()
