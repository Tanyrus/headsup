import unittest

from tooling import load

abi = load('tools/abi_check.py')

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

    def test_only_overloads_and_variadics_are_rejected(self):
        self.assertEqual(abi.unstable_methods(self.methods(SAMPLE)),
                         {'Pos': 'overloaded', 'Size': 'overloaded', 'Log': 'variadic'})

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


class UnlistedInterfaces(unittest.TestCase):
    TEXT = """
struct ICore
{
    virtual IFonts* GetFonts(void) = 0;
    virtual IChat* GetChat(void) = 0;
};
struct IFonts
{
    virtual void Draw(void) = 0;
};
struct IChat
{
    virtual void Write(void) = 0;
};
"""

    def unlisted(self, sources, calls, listed):
        return abi.unlisted_interfaces(sources, self.TEXT, abi.interfaces(self.TEXT), calls, listed)

    def test_an_interface_named_or_reached_must_be_listed(self):
        self.assertEqual(self.unlisted('IChat* chat;', set(), ['ICore']), ['IChat'])
        self.assertEqual(self.unlisted('', {'GetFonts'}, ['ICore']), ['IFonts'])
        self.assertEqual(self.unlisted('IChat* chat;', {'GetChat'}, ['ICore', 'IChat']), [])

    def test_a_pointer_to_something_that_is_not_an_interface_is_ignored(self):
        self.assertEqual(self.unlisted('IDirect3DDevice8* device;', set(), ['ICore']), [])


if __name__ == '__main__':
    unittest.main()
