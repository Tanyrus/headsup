#include "test.h"

int main()
{
    int failed = 0;
    for (const test::Case& c : test::Registry())
    {
        try
        {
            c.fn();
            std::printf("PASS %s\n", c.name);
        }
        catch (const test::Failure& f)
        {
            ++failed;
            std::printf("FAIL %s\n     %s\n", c.name, f.message.c_str());
        }
    }
    std::printf("\n%zu tests, %d failed\n", test::Registry().size(), failed);
    return failed == 0 ? 0 : 1;
}
