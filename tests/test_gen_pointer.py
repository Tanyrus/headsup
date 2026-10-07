import pathlib
import tempfile
import unittest

from tooling import load

gen = load('tools/gen_pointer.py')


class Generate(unittest.TestCase):
    def test_the_cursor_file_is_embedded_byte_for_byte(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'pointer.ani'
            path.write_bytes(bytes(range(18)))
            text = gen.generate(path)
        self.assertIn('const unsigned char kChocoboPointerFile[] = {\n'
                      '    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,\n'
                      '    0x10, 0x11,\n};', text)

    def test_a_missing_file_fails(self):
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(gen.PointerError):
            gen.generate(pathlib.Path(tmp) / 'missing.ani')


if __name__ == '__main__':
    unittest.main()
