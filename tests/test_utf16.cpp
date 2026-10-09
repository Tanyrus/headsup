#include "utf16.h"
#include "test.h"

using namespace headsup;

TEST(utf8_becomes_utf16)
{
    CHECK(Utf16("Lv 22") == std::wstring(L"Lv 22"));
    CHECK(Utf16("\xC2\xB7") == std::wstring(L"\x00B7"));               // a middle dot
    CHECK(Utf16("a\xE2\x82\xAC") == std::wstring(L"a\x20AC"));         // three bytes: the euro sign
    CHECK(Utf16("\xF0\x9F\x90\xA6") == std::wstring(L"\xD83D\xDC26")); // above U+FFFF: a surrogate pair
    CHECK(Utf16("") == std::wstring());
}

TEST(broken_utf8_becomes_replacement_characters_rather_than_nothing)
{
    CHECK(Utf16("\xFF") == std::wstring(L"\xFFFD"));          // never a valid byte
    CHECK(Utf16("a\xC2") == std::wstring(L"a\xFFFD"));        // cut short
    CHECK(Utf16("a\xC2z") == std::wstring(L"a\xFFFDz"));      // missing its continuation
    CHECK(Utf16("\x80\x80") == std::wstring(L"\xFFFD\xFFFD")); // continuations with no lead
    CHECK(Utf16("\xF4\x90\x80\x80") == std::wstring(L"\xFFFD"));  // past U+10FFFF, the last code point
}
