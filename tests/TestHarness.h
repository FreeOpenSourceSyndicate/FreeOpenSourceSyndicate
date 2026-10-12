/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Minimal, dependency-free test harness.
//
// Tests self-register at static-init time, so adding a test file does not
// require editing any other file (important when several agents add tests in
// parallel).
//
//   FOSS_TEST(Isometric, RoundTrip) { ... FOSS_CHECK_EQ(a, b); }

namespace foss::test
{

struct TestCase
{
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

inline int& failureCount()
{
    static int failures = 0;
    return failures;
}

inline bool registerTest(const char* suite, const char* name, std::function<void()> fn)
{
    registry().push_back(TestCase{suite, name, std::move(fn)});
    return true;
}

inline void reportFailure(const char* file, int line, const std::string& message)
{
    ++failureCount();
    std::printf("    FAIL %s:%d: %s\n", file, line, message.c_str());
}

inline bool nearlyEqual(float a, float b, float epsilon = 1e-4f)
{
    return std::fabs(a - b) <= epsilon;
}

} // namespace foss::test

#define FOSS_TEST(suite, name)                                                                     \
    static void foss_test_##suite##_##name();                                                      \
    static const bool foss_reg_##suite##_##name =                                                  \
        ::foss::test::registerTest(#suite, #name, foss_test_##suite##_##name);                     \
    static void foss_test_##suite##_##name()

#define FOSS_CHECK(cond)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ::foss::test::reportFailure(__FILE__, __LINE__, "expected: " #cond);                   \
        }                                                                                          \
    } while (0)

#define FOSS_CHECK_EQ(a, b)                                                                        \
    do {                                                                                           \
        if (!((a) == (b))) {                                                                       \
            ::foss::test::reportFailure(__FILE__, __LINE__, "expected " #a " == " #b);             \
        }                                                                                          \
    } while (0)

#define FOSS_CHECK_NEAR(a, b)                                                                      \
    do {                                                                                           \
        if (!::foss::test::nearlyEqual((a), (b))) {                                                \
            ::foss::test::reportFailure(__FILE__, __LINE__, "expected " #a " ~= " #b);             \
        }                                                                                          \
    } while (0)
