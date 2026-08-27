/**
 * @file test_runner.c
 * @brief Aggregates module-level tests and adapts framework output to the board/host callback.
 */
#include "test_runner.h"
#include "unity.h"

#include <stddef.h>

void RunAudioProcessingTests(void);
void RunBtProfileTests(void);
void RunConnectivityControllerTests(void);
void RunTelecoilDetectorTests(void);
void RunTelecoilFilterTests(void);
void RunSignalQualityTests(void);
void RunConnectivityServiceTests(void);
void RunControlProtocolTests(void);
void RunPcmTransportTests(void);
void RunAudioDynamicsTests(void);
void RunConnectivityActionsTests(void);
void RunRequirementsTimingTests(void);

static TestOutputFn g_output_fn;
static char g_output_buffer[192];
static size_t g_output_length;

/** @brief Flush buffered framework characters through the configured output callback. */
static void flush_buffer(void)
{
    if ((g_output_fn != NULL) && (g_output_length > 0U)) {
        g_output_buffer[g_output_length] = '\0';
        g_output_fn(g_output_buffer);
    }
    g_output_length = 0U;
}

/**
 * @brief Buffer one framework output character and translate LF to CRLF for serial terminals.
 * @param c Character emitted by the test framework.
 * @return The same character value.
 */
int UnityOutputChar(int c)
{
    if (g_output_length >= (sizeof(g_output_buffer) - 2U)) {
        flush_buffer();
    }
    if (c == '\n') {
        g_output_buffer[g_output_length++] = '\r';
    }
    g_output_buffer[g_output_length++] = (char)c;
    if (c == '\n') {
        flush_buffer();
    }
    return c;
}

/** @brief Flush any pending framework output characters. */
void UnityOutputFlush(void)
{
    flush_buffer();
}

/** @brief Per-test setup hook; individual tests construct their own isolated local fixtures. */
void setUp(void)
{
    /* Tests construct their own local fixtures to guarantee isolation. */
}

/** @brief Per-test teardown hook; no dynamic resources require cleanup. */
void tearDown(void)
{
    /* No dynamically allocated resources are used by the application tests. */
}

/** @copydoc RunAllTests */
int RunAllTests(TestOutputFn output_fn)
{
    g_output_fn = output_fn;
    g_output_length = 0U;

    (void)UNITY_BEGIN();
    RunConnectivityControllerTests();
    RunBtProfileTests();
    RunAudioProcessingTests();
    RunTelecoilDetectorTests();
    RunTelecoilFilterTests();
    RunSignalQualityTests();
    RunConnectivityServiceTests();
    RunControlProtocolTests();
    RunPcmTransportTests();
    RunAudioDynamicsTests();
    RunConnectivityActionsTests();
    RunRequirementsTimingTests();
    const int failures = UNITY_END();
    UnityOutputFlush();
    return failures;
}
