import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location('abi_check', ROOT / 'tools' / 'abi_check.py')
abi = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(abi)

SAMPLE = """
struct IThing
{
    virtual void A(void) = 0;
    virtual void Pos(int x) = 0;
    virtual void Size(int x) = 0;
    virtual void Pos(const char* name, int x) = 0;
    virtual void Size(const char* name, int x) = 0;
    virtual void B(int v) const = 0;
    virtual void Log(const char* fmt, ...) = 0;
};
"""


class Layouts(unittest.TestCase):
    def methods(self, text):
        return abi.interfaces(text)['IThing']

    def test_parses_declarations_in_order(self):
        self.assertEqual([n for n, _ in self.methods(SAMPLE)], ['A', 'Pos', 'Size', 'Pos', 'Size', 'B', 'Log'])

    def test_rejects_overloads_and_variadics(self):
        bad = abi.unstable_methods(self.methods(SAMPLE))
        self.assertEqual(bad['Pos'], 'overloaded')
        self.assertEqual(bad['Size'], 'overloaded')
        self.assertEqual(bad['Log'], 'variadic')

    def test_methods_outside_split_groups_keep_their_slot(self):
        bad = abi.unstable_methods(self.methods(SAMPLE))
        self.assertNotIn('A', bad)
        self.assertNotIn('B', bad)

    def test_method_between_split_overloads_moves(self):
        text = SAMPLE.replace('virtual void Size(int x) = 0;', 'virtual void Mid(int x) = 0;')
        text = text.replace('virtual void Size(const char* name, int x) = 0;', '')
        bad = abi.unstable_methods(self.methods(text))  # A, Pos, Mid, Pos: MSVC moves Mid from slot 2 to 3
        self.assertEqual(bad['Mid'], 'slot 2 under MinGW but 3 under MSVC')


class StructReturns(unittest.TestCase):
    TEXT = """
struct Vec2
{
    float x, y;
};
struct IGui
{
    virtual IMGUI_API Vec2 GetSize(void) const = 0;
    virtual IMGUI_API Vec2* GetSizePointer(void) = 0;
    virtual IMGUI_API const Vec2& GetSizeReference(void) = 0;
    virtual IMGUI_API float GetWidth(void) = 0;
    virtual IMGUI_API void SetSize(const Vec2& size) = 0;
};
"""

    def test_only_a_struct_returned_by_value_is_rejected(self):
        # MSVC returns it through a hidden pointer from a member function; MinGW expects it in registers.
        self.assertEqual(abi.struct_returns(self.TEXT), {'IGui': {'GetSize'}})


if __name__ == '__main__':
    unittest.main()
