"""Circler logic regressions: no GL display or Qt runtime needed."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import types
import unittest

SRC = Path(__file__).resolve().parents[1] / "musik" / "circler_reactive.py"
try:
    import moderngl
except ImportError:
    sys.modules['moderngl'] = types.ModuleType('moderngl')

spec = importlib.util.spec_from_file_location('musik_circler_reactive_test', SRC)
scene = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = scene
spec.loader.exec_module(scene)


class CirclerTests(unittest.TestCase):
    def test_tetra_growth_and_trajectory_unchanged(self):
        tetra = scene.Tetrahedron((255, 110, 0))
        tetra.update(1.0)
        self.assertEqual(tetra.size, 7.0)
        self.assertEqual(tetra.radius, 20.0)
        self.assertEqual(tetra.angle, 2.0)
        self.assertAlmostEqual(tetra.rotation, 1.0471975512)

    def test_16_bass_hits_change_palette_only_for_new_tetrahedra(self):
        s = scene.CirclerScene(seed=5)
        first_color = s.tetrahedra[0].color
        self.assertEqual(scene.PALETTE_BEATS, 16)
        for i in range(15):
            s.elapsed = i * .4
            self.assertTrue(s.beat())
        self.assertEqual(s.current_palette, scene.INITIAL_PALETTE)
        s.elapsed = 15 * .4
        self.assertTrue(s.beat())
        self.assertIn(s.current_palette, scene.NEON_PALETTES)
        self.assertEqual(s.tetrahedra[0].color, first_color)
        self.assertNotEqual(s._spawn().color, first_color)
        s.elapsed = 16 * .4
        self.assertTrue(s.beat())
        self.assertEqual(s.total_beats, 17)

    def test_bass_jump_20_percent_stronger_than_v2(self):
        s = scene.CirclerScene(seed=2)
        t = s.tetrahedra[0]
        t.size = 20.0
        expected_old = 16.0 + 20.0 * .38
        self.assertTrue(s.beat())
        self.assertAlmostEqual(scene.BASS_KICK_MULTIPLIER, 1.8)
        self.assertAlmostEqual(t.kick, 1.2 * 1.5 * expected_old)
        old_radius = t.radius
        t.update(.10)
        self.assertGreater(t.radius, old_radius)
        self.assertLess(t.kick, 1.8 * expected_old)

    def test_background_cycle(self):
        s = scene.CirclerScene()
        for seconds, expected in ((0,(20,20,40)), (12,(40,40,20)),
                                  (24,(40,20,20)),(36,(20,20,40))):
            s.elapsed = seconds
            self.assertEqual(s.background_color(),expected)

    def test_background_radial_gradient_drawn_before_tetrahedra(self):
        shader = scene.BACKGROUND_FRAGMENT_SHADER
        self.assertIn("gl_FragCoord.xy - half_size", shader)
        self.assertIn("distance_to_edge", shader)
        self.assertIn("smoothstep(0.07, 1.0, radius)", shader)
        self.assertIn("edge_color * brightness", shader)

    def test_background_palette_kept(self):
        self.assertEqual(scene.BACKGROUND_COLORS,
                         ((20, 20, 40), (40, 40, 20), (40, 20, 20)))

    def test_rapid_duplicate_beats_debounced(self):
        s = scene.CirclerScene()
        self.assertTrue(s.beat())
        self.assertFalse(s.beat())
        self.assertEqual(s.total_beats,1)

    def test_same_tetrahedron_mesh(self):
        self.assertEqual(len(scene.tetrahedron_vertices()), 12 * 6 * 4)

    def test_bounds_spawn_and_remove(self):
        s = scene.CirclerScene(seed=9)
        for _ in range(3):
            s.update(.10, 1280, 720)
        self.assertEqual(len(s.tetrahedra), 2)
        s.tetrahedra[0].radius = 100_000
        s.update(.01, 1280, 720)
        self.assertEqual(len(s.tetrahedra),1)


if __name__ == '__main__':
    unittest.main()
