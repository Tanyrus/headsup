#pragma once

// Minimal test framework: TEST(name) registers a case, CHECK/CHECK_EQ throw on failure.
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace test
{
    struct Case
    {
        const char* name;
        std::function<void()> fn;
    };

    inline std::vector<Case>& Registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    struct Register
    {
        Register(const char* name, std::function<void()> fn) { Registry().push_back({name, std::move(fn)}); }
    };

    struct Failure
    {
        std::string message;
    };

    inline std::string Where(const char* file, int line) { return std::string(file) + ":" + std::to_string(line) + ": "; }
}

#define TEST_CAT2(a, b) a##b
#define TEST_CAT(a, b) TEST_CAT2(a, b)
#define TEST(name)                                                    \
    static void name();                                               \
    static const test::Register TEST_CAT(register_, name)(#name, name); \
    static void name()

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond)) throw test::Failure{test::Where(__FILE__, __LINE__) + "CHECK(" #cond ")"}; \
    } while (0)

#define CHECK_EQ(a, b)                                                                                 \
    do                                                                                                 \
    {                                                                                                  \
        const auto va_ = (a);                                                                          \
        const auto vb_ = (b);                                                                          \
        if (!(va_ == vb_))                                                                             \
            throw test::Failure{test::Where(__FILE__, __LINE__) + "CHECK_EQ(" #a ", " #b "): " +       \
                                std::to_string(va_) + " != " + std::to_string(vb_)};                   \
    } while (0)
