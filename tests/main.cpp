/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <cstdio>

#include "TestHarness.h"

int main()
{
    int passed = 0;
    for (const auto& test : fossynd::test::registry())
    {
        std::printf("[ RUN  ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        const int before = fossynd::test::failureCount();
        test.fn();
        if (fossynd::test::failureCount() == before)
        {
            ++passed;
            std::printf("[  OK  ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        }
        else
        {
            std::printf("[ FAIL ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        }
    }

    const int total = static_cast<int>(fossynd::test::registry().size());
    std::printf("\n%d/%d tests passed, %d assertion failure(s)\n", passed, total, fossynd::test::failureCount());
    return fossynd::test::failureCount() == 0 ? 0 : 1;
}
