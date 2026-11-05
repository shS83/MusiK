"""Logic tests; can run without a GL driver or even the moderngl package."""
import importlib
import pathlib
import sys
import types
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
if 'moderngl' not in sys.modules:
    try:
        import moderngl
    except ImportError:
        sys.modules['moderngl'] = types.ModuleType('moderngl')
scene = importlib.import_module('circler_reactive')


class CirclerTests(unittest.TestCase):
    def test_original_spiral_and_growth(self):
        t = scene.Tetrahedron((255, 0, 0))
        t.update(1.0)
        self.assertAlmostEqual(t.size, 7.0)
        self.assertAlmostEqual(t.radius, 20.0)
        self.assertAlmostEqual(t.angle, 2.0)

    def test_background_cycle(self):
        s = scene.CirclerScene(seed=1)
        self.assertEqual(s.background_color(), (20, 20, 40))
        s.elapsed = 12.0
        self.assertEqual(s.background_color(), (40, 40, 20))
        s.elapsed = 24.0
        self.assertEqual(s.background_color(), (40, 20, 20))
        s.elapsed = 36.0
        self.assertEqual(s.background_color(), (20, 20, 40))

    def test_64_beats_switch_palette_but_not_existing_colors(self):
        s = scene.CirclerScene(seed=5)
        color = s.tetrahedra[0].color
        for i in range(64):
            s.elapsed = i * .5
            self.assertTrue(s.beat())
        self.assertNotEqual(s.current_palette, scene.INITIAL_PALETTE)
        self.assertEqual(s.total_beats, 64)
        self.assertEqual(s.tetrahedra[0].color, color)
        new_tetra = s._spawn()
        self.assertNotEqual(new_tetra.color, color)

    def test_debounce(self):
        s = scene.CirclerScene(seed=2)
        self.assertTrue(s.beat())
        self.assertFalse(s.beat())
        self.assertEqual(s.total_beats, 1)

    def test_original_geometry_has_12_vertices(self):
        self.assertEqual(len(scene.tetrahedron_vertices()), 12 * 6 * 4)


if __name__ == '__main__':
    unittest.main()
