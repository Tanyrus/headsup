import pathlib
import tempfile
import unittest

from tooling import load

gen = load('tools/gen_shapes.py')


class Flatten(unittest.TestCase):
    def test_lines_become_points_inside_the_unit_box(self):
        points, aspect = gen.polygon('M10 10L30 10L30 50Z')
        self.assertEqual(points, [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0)])
        self.assertEqual(aspect, 2.0)  # 40 tall, 20 wide

    def test_a_curve_is_split_into_segments_through_its_points(self):
        # A symmetric arch from (0,0) to (4,0) peaking at y=3 at t = 0.5.
        points, _ = gen.polygon('M0 0C0 4 4 4 4 0Z')
        self.assertEqual(len(points), gen.SEGMENTS + 1)  # the start, then each segment's end
        self.assertEqual(points[0], (0.0, 0.0))
        self.assertEqual(points[-1], (1.0, 0.0))
        self.assertEqual(points[gen.SEGMENTS // 2], (0.5, 1.0))  # the middle of the curve is its peak

    def test_only_absolute_moves_lines_and_cubics_are_read(self):
        for path in ('M0 0l10 10Z', 'M0 0Q5 5 10 0Z', 'M0 0L10 0L10 10M20 20L30 30Z', 'M0 0L10Z', ''):
            with self.subTest(path=path), self.assertRaises(gen.ShapeError):
                gen.polygon(path)


class Generate(unittest.TestCase):
    def test_the_svg_path_becomes_a_shape(self):
        with tempfile.TemporaryDirectory() as tmp:
            svg = pathlib.Path(tmp) / 'shape.svg'
            svg.write_text('<svg><g><path d="M0 0L2 0L2 4Z" fill="white"/></g></svg>')
            text = gen.generate(svg)
        self.assertIn('const ShapePoint kFeatherPoints[] = {\n    {0.0000f, 0.0000f},\n    {1.0000f, 0.0000f},\n'
                      '    {1.0000f, 1.0000f},\n};', text)
        self.assertIn('constexpr float kFeatherAspect = 2.0000f;', text)

    def test_an_svg_without_a_path_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            svg = pathlib.Path(tmp) / 'shape.svg'
            svg.write_text('<svg></svg>')
            with self.assertRaises(gen.ShapeError):
                gen.generate(svg)


if __name__ == '__main__':
    unittest.main()
