import unittest

import numpy as np

from musik.biolume import Biolume, FLY_COLORS


class BiolumeTests(unittest.TestCase):
    def test_each_firefly_color_contributes_to_the_accumulated_pigment(self):
        bio = Biolume(16 / 9)
        original = bio.pigment_sum[0].copy()
        bio.flies[:4, :2] = bio.cells[0, :2]
        bio.step(.016, 16 / 9, 0, 0, 0)

        np.testing.assert_array_equal(bio.owner[:4], np.zeros(4, dtype=int))
        np.testing.assert_allclose(bio.pigment_sum[0], original + FLY_COLORS.sum(axis=0))
        self.assertEqual(bio.pigment_weight[0], 11)
        np.testing.assert_allclose(bio.colors[0], bio.pigment_sum[0] / 11)

    def test_merging_pulls_both_cells_together_and_combines_their_pigments(self):
        bio = Biolume(16 / 9)
        bio.cells[0, :3] = (-.04, 0, .08)
        bio.cells[1, :3] = (.04, 0, .06)
        bio.velocity[:2] = 0
        combined_pigment = bio.pigment_sum[:2].sum(axis=0)
        combined_weight = bio.pigment_weight[:2].sum()

        bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertEqual(bio.merge_target[1], 0)
        before = bio.cells[:2, 0].copy()
        bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertGreater(bio.cells[0, 0], before[0])
        self.assertLess(bio.cells[1, 0], before[1])

        for _ in range(50):
            bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertEqual(bio.merge_target[1], -1)
        np.testing.assert_allclose(bio.pigment_sum[0], combined_pigment)
        self.assertEqual(bio.pigment_weight[0], combined_weight)
        np.testing.assert_allclose(bio.colors[0], combined_pigment / combined_weight)

    def test_giant_bursts_into_tiny_cells_with_its_pigment(self):
        bio = Biolume(16 / 9)
        bio.cells[0, 2] = .17
        bio.flies[:, :2] = 10
        bio.colors[0] = (.8, .2, .1)
        bio.pigment_sum[0] = bio.colors[0]*bio.pigment_weight[0]
        parent_color = bio.colors[0].copy()

        bio.step(.016, 16 / 9, 0, 0, 0)
        active = bio.fragments[:, 3] > 0
        self.assertEqual(active.sum(), 36)
        self.assertLess(bio.cells[0, 2], .07)
        self.assertTrue(abs(bio.cells[0, 0]) > 16/9*.5 or abs(bio.cells[0, 1]) > .5)
        np.testing.assert_allclose(bio.burst_colors[0], parent_color)
        self.assertEqual((bio.sparks[:, 3] > 0).sum(), 24)
        before = bio.fragments[active, :2].copy()
        bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertTrue(abs(bio.cells[0, 0]) > 16/9*.5 or abs(bio.cells[0, 1]) > .5)
        self.assertGreater(np.linalg.norm(bio.fragments[active, :2]-before, axis=1).mean(), .001)

        pixel_sized = False
        for _ in range(250):
            bio.step(.016, 16 / 9, 0, 0, 0)
            pixel_sized |= bool(((bio.fragments[:, 3] > 0) &
                                 (bio.fragments[:, 2] <= 1/720)).any())
        self.assertTrue(pixel_sized)
        self.assertFalse((bio.fragments[:, 3] > 0).any())

    def test_second_burst_keeps_all_fragments_from_the_first(self):
        bio = Biolume(16 / 9)
        bio.flies[:, :2] = 10
        bio.cells[0, :3] = (-.12, 0, .17)
        bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertEqual((bio.fragments[:36, 3] > 0).sum(), 36)

        bio.cells[1, :3] = (.12, 0, .17)
        bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertEqual((bio.fragments[:36, 3] > 0).sum(), 36)
        self.assertEqual((bio.fragments[36:, 3] > 0).sum(), 36)
        self.assertEqual(bio.burst_count, 2)

        bio.cells[2, :3] = (0, 0, .17)
        self.assertFalse(bio.burst(2, 16 / 9))
        self.assertEqual(bio.burst_count, 2)
        self.assertEqual((bio.fragments[:, 3] > 0).sum(), 72)

    def test_growth_reaches_burst_threshold_without_a_merge(self):
        bio = Biolume(16 / 9)
        bio.cells[0, 2] = .129
        bio.flies[:, :2] = 10
        for _ in range(80):
            bio.step(.016, 16 / 9, 0, 0, 0)
        self.assertGreater(bio.burst_count, 0)


if __name__ == "__main__":
    unittest.main()
