/**
 * @file test_runner.h
 * @brief Entry point for the primary 256-test automated unit-test suite.
 */
#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

/**
 * @brief Callback used to stream test-framework text to a host terminal or board console.
 * @param text NUL-terminated output text.
 */
typedef void (*TestOutputFn)(const char *text);

/**
 * @brief Run every registered application unit-test group.
 * @param output_fn Output callback used by the test framework; may be NULL for silent execution.
 * @return Number of failed tests; zero means all tests passed.
 */
int RunAllTests(TestOutputFn output_fn);

#endif
