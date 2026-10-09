# Test configuration
enable_testing()

# Test sources
set(TEST_SOURCES
    unit_tests.cpp
)

# Create test executable
add_executable(run_tests ${TEST_SOURCES})

# Add tests
add_test(NAME unit_tests COMMAND run_tests)
