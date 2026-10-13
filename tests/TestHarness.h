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
//   FOSSYND_TEST(Isometric, RoundTrip) { ... FOSSYND_CHECK_EQ(a, b); }

namespace fossynd::test
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

} // namespace fossynd::test

#define FOSSYND_TEST(suite, name)                                                                     \
    static void fossynd_test_##suite##_##name();                                                      \
    static const bool fossynd_reg_##suite##_##name =                                                  \
        ::fossynd::test::registerTest(#suite, #name, fossynd_test_##suite##_##name);                     \
    static void fossynd_test_##suite##_##name()

#define FOSSYND_CHECK(cond)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ::fossynd::test::reportFailure(__FILE__, __LINE__, "expected: " #cond);                   \
        }                                                                                          \
    } while (0)

#define FOSSYND_CHECK_EQ(a, b)                                                                        \
    do {                                                                                           \
        if (!((a) == (b))) {                                                                       \
            ::fossynd::test::reportFailure(__FILE__, __LINE__, "expected " #a " == " #b);             \
        }                                                                                          \
    } while (0)

#define FOSSYND_CHECK_NEAR(a, b)                                                                      \
    do {                                                                                           \
        if (!::fossynd::test::nearlyEqual((a), (b))) {                                                \
            ::fossynd::test::reportFailure(__FILE__, __LINE__, "expected " #a " ~= " #b);             \
        }                                                                                          \
    } while (0)
