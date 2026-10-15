#include "json_text.h"

#include <algorithm>
#include <cstdlib>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kHighHalf = 0xD800, kLowHalf = 0xDC00, kPastHalves = 0xE000, kHalfBits = 10, kPastPlane = 0x10000;

        void AppendUtf8(std::string& out, uint32_t code)
        {
            if (code < 0x80)
                out += static_cast<char>(code);
            else if (code < 0x800)
            {
                out += static_cast<char>(0xC0 | code >> 6);
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
            else if (code < kPastPlane)
            {
                out += static_cast<char>(0xE0 | code >> 12);
                out += static_cast<char>(0x80 | (code >> 6 & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | code >> 18);
                out += static_cast<char>(0x80 | (code >> 12 & 0x3F));
                out += static_cast<char>(0x80 | (code >> 6 & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
        }

        class Parser
        {
        public:
            explicit Parser(std::string_view text) : m_Text(text) {}

            std::optional<JsonValue> Document(std::string& problem)
            {
                std::optional<JsonValue> value = Value(0);
                Space();
                if (value && m_At != m_Text.size()) Fail("text after the end");
                if (!m_Problem.empty())
                {
                    problem = m_Problem;
                    return std::nullopt;
                }
                return value;
            }

        private:
            std::string_view m_Text;
            size_t m_At = 0;
            std::string m_Problem;

            void Fail(const char* what)
            {
                if (!m_Problem.empty()) return;
                const auto line = 1 + std::count(m_Text.begin(), m_Text.begin() + static_cast<std::ptrdiff_t>(m_At), '\n');
                m_Problem = "line " + std::to_string(line) + ": " + what;
            }
            bool More() const { return m_At < m_Text.size(); }
            char Next() const { return More() ? m_Text[m_At] : '\0'; }
            void Space()
            {
                while (More() && (Next() == ' ' || Next() == '\t' || Next() == '\n' || Next() == '\r'))
                    ++m_At;
            }
            bool Take(char c)
            {
                Space();
                if (Next() != c) return false;
                ++m_At;
                return true;
            }
            bool Word(std::string_view word)
            {
                if (m_Text.substr(m_At, word.size()) != word) return false;
                m_At += word.size();
                return true;
            }

            std::optional<JsonValue> Value(size_t depth)
            {
                Space();
                JsonValue value;
                const char c = Next();
                if (c == '{' || c == '[')
                {
                    if (depth >= kMostJsonDepth)
                    {
                        Fail("lists and objects nested too deep");
                        return std::nullopt;
                    }
                    return c == '{' ? Object(depth + 1) : List(depth + 1);
                }
                if (c == '"')
                {
                    value.kind = JsonValue::Kind::Text;
                    if (!Text(value.text)) return std::nullopt;
                    return value;
                }
                const bool yes = Word("true");
                if (yes || Word("false"))
                {
                    value.kind = JsonValue::Kind::Flag;
                    value.flag = yes;
                    return value;
                }
                if (Word("null")) return value;
                if (c == '-' || (c >= '0' && c <= '9')) return Number();
                Fail(More() ? "expected a value" : "the text ends where a value was expected");
                return std::nullopt;
            }

            std::optional<JsonValue> Number()
            {
                const size_t start = m_At;
                const auto digits  = [&] {
                    const size_t from = m_At;
                    while (Next() >= '0' && Next() <= '9')
                        ++m_At;
                    return m_At - from;
                };
                if (Next() == '-') ++m_At;
                const size_t whole = m_At;
                if (digits() == 0 || (m_Text[whole] == '0' && m_At - whole > 1))
                {
                    Fail("a number is written wrong");
                    return std::nullopt;
                }
                if (Next() == '.' && (++m_At, digits() == 0))
                {
                    Fail("a number is written wrong");
                    return std::nullopt;
                }
                if (Next() == 'e' || Next() == 'E')
                {
                    ++m_At;
                    if (Next() == '+' || Next() == '-') ++m_At;
                    if (digits() == 0)
                    {
                        Fail("a number is written wrong");
                        return std::nullopt;
                    }
                }
                JsonValue value;
                value.kind   = JsonValue::Kind::Number;
                value.number = std::strtod(std::string(m_Text.substr(start, m_At - start)).c_str(), nullptr);
                return value;
            }

            std::optional<uint32_t> Hex4()
            {
                if (m_At + 4 > m_Text.size()) return std::nullopt;
                uint32_t code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char h = m_Text[m_At++];
                    const int digit = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                    if (digit < 0) return std::nullopt;
                    code = code << 4 | static_cast<uint32_t>(digit);
                }
                return code;
            }

            bool Text(std::string& out)
            {
                ++m_At; // the opening quote
                while (true)
                {
                    if (!More())
                    {
                        Fail("the text ends inside a string");
                        return false;
                    }
                    const char c = m_Text[m_At++];
                    if (c == '"') return true;
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        Fail("a string holds a control character");
                        return false;
                    }
                    if (c != '\\')
                    {
                        out += c;
                        continue;
                    }
                    const char e = More() ? m_Text[m_At++] : '\0';
                    switch (e)
                    {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u':
                    {
                        std::optional<uint32_t> code = Hex4();
                        if (code && *code >= kHighHalf && *code < kLowHalf)
                        {
                            const std::optional<uint32_t> low = Word("\\u") ? Hex4() : std::nullopt;
                            code = low && *low >= kLowHalf && *low < kPastHalves
                                       ? std::optional(kPastPlane + ((*code - kHighHalf) << kHalfBits) + (*low - kLowHalf))
                                       : std::nullopt;
                        }
                        else if (code && *code >= kLowHalf && *code < kPastHalves)
                            code = std::nullopt;
                        if (!code)
                        {
                            Fail("a string holds a \\u escape that is no character");
                            return false;
                        }
                        AppendUtf8(out, *code);
                        break;
                    }
                    default: Fail("a string holds an unknown escape"); return false;
                    }
                }
            }

            std::optional<JsonValue> List(size_t depth)
            {
                ++m_At;
                JsonValue list;
                list.kind = JsonValue::Kind::List;
                if (Take(']')) return list;
                do
                {
                    std::optional<JsonValue> item = Value(depth);
                    if (!item) return std::nullopt;
                    list.items.push_back(std::move(*item));
                } while (Take(','));
                if (Take(']')) return list;
                Fail("expected ',' or ']'");
                return std::nullopt;
            }

            std::optional<JsonValue> Object(size_t depth)
            {
                ++m_At;
                JsonValue object;
                object.kind = JsonValue::Kind::Object;
                if (Take('}')) return object;
                do
                {
                    Space();
                    std::string name;
                    if (Next() != '"')
                    {
                        Fail("expected a field's name in quotes");
                        return std::nullopt;
                    }
                    if (!Text(name)) return std::nullopt;
                    if (!Take(':'))
                    {
                        Fail("expected ':' after a field's name");
                        return std::nullopt;
                    }
                    std::optional<JsonValue> item = Value(depth);
                    if (!item) return std::nullopt;
                    object.names.push_back(std::move(name));
                    object.items.push_back(std::move(*item));
                } while (Take(','));
                if (Take('}')) return object;
                Fail("expected ',' or '}'");
                return std::nullopt;
            }
        };
    }

    const JsonValue* JsonValue::Field(std::string_view name) const
    {
        for (size_t i = names.size(); i-- > 0;)
            if (names[i] == name) return &items[i];
        return nullptr;
    }

    std::optional<JsonValue> ParseJson(std::string_view text, std::string& problem)
    {
        return Parser(text).Document(problem);
    }
}
