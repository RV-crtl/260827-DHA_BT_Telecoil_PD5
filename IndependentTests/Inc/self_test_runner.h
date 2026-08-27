/**
 * @file self_test_runner.h
 * @brief Entry point for the supplementary framework-independent regression suite.
 */
#ifndef SELF_TEST_RUNNER_H
#define SELF_TEST_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback used to stream SelfTest output to a host terminal or board console.
 * @param text NUL-terminated output text.
 */
typedef void (*SelfTestOutputFn)(const char *text);

/**
 * @brief Run all independent acceptance/regression checks over the portable application.
 *
 * This suite is deliberately separate from the primary automated unit-test framework and
 * provides a second implementation of key behavioural checks on either STM32 or host PC.
 *
 * @param output_fn Output callback; may be NULL for silent execution.
 * @return Number of failed checks; zero means every check passed.
 */
int SelfTest_RunAll(SelfTestOutputFn output_fn);

#ifdef __cplusplus
}
#endif

#endif
