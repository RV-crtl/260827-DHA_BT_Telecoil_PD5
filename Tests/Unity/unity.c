#include "unity.h"

#include <stddef.h>

UNITY_STORAGE_T Unity;

static void unity_putc(char c)
{
    (void)UNITY_OUTPUT_CHAR((int)c);
}

static void unity_puts(const char *text)
{
    if (text == NULL) {
        text = "NULL";
    }
    while (*text != '\0') {
        unity_putc(*text++);
    }
}

static void unity_print_u64(uint64_t value)
{
    char digits[24];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));
    while (count > 0U) {
        unity_putc(digits[--count]);
    }
}

static void unity_print_i64(int64_t value)
{
    uint64_t magnitude;
    if (value < 0) {
        unity_putc('-');
        magnitude = (uint64_t)(-(value + 1));
        magnitude += 1U;
    } else {
        magnitude = (uint64_t)value;
    }
    unity_print_u64(magnitude);
}

static void unity_print_float(float value)
{
    if (value < 0.0f) {
        unity_putc('-');
        value = -value;
    }
    const uint32_t whole = (uint32_t)value;
    float fraction_f = (value - (float)whole) * 100000.0f;
    uint32_t fraction = (uint32_t)(fraction_f + 0.5f);
    if (fraction >= 100000U) {
        unity_print_u64((uint64_t)whole + 1U);
        unity_puts(".00000");
        return;
    }
    unity_print_u64(whole);
    unity_putc('.');
    uint32_t divisor = 10000U;
    while (divisor > 0U) {
        unity_putc((char)('0' + ((fraction / divisor) % 10U)));
        divisor /= 10U;
    }
}

static void unity_print_test_prefix(UNITY_LINE_TYPE line)
{
    unity_puts((Unity.TestFile != NULL) ? Unity.TestFile : "test");
    unity_putc(':');
    unity_print_u64((uint64_t)line);
    unity_putc(':');
    unity_puts((Unity.CurrentTestName != NULL) ? Unity.CurrentTestName : "unknown_test");
    unity_putc(':');
}

static bool unity_fail_at(const char *message, UNITY_LINE_TYPE line)
{
    if (Unity.CurrentTestFailed == 0U) {
        Unity.CurrentTestFailed = 1U;
        unity_print_test_prefix(line);
        unity_puts("FAIL: ");
        unity_puts(message);
        unity_putc('\n');
        UNITY_OUTPUT_FLUSH();
    }
    return false;
}

void UnityBegin(const char *filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0U;
    Unity.NumberOfTests = 0U;
    Unity.TestFailures = 0U;
    Unity.TestIgnores = 0U;
    Unity.CurrentTestFailed = 0U;
    Unity.CurrentTestIgnored = 0U;
}

void UnityDefaultTestRun(void (*func)(void), const char *name, UNITY_LINE_TYPE line)
{
    Unity.CurrentTestName = name;
    Unity.CurrentTestLineNumber = line;
    Unity.CurrentTestFailed = 0U;
    Unity.CurrentTestIgnored = 0U;
    Unity.NumberOfTests++;

    setUp();
    func();
    tearDown();

    if (Unity.CurrentTestIgnored != 0U) {
        Unity.TestIgnores++;
    } else if (Unity.CurrentTestFailed != 0U) {
        Unity.TestFailures++;
    } else {
        unity_print_test_prefix(line);
        unity_puts("PASS\n");
    }
    UNITY_OUTPUT_FLUSH();
}

int UnityEnd(void)
{
    unity_puts("-----------------------\n");
    unity_print_u64(Unity.NumberOfTests);
    unity_puts(" Tests ");
    unity_print_u64(Unity.TestFailures);
    unity_puts(" Failures ");
    unity_print_u64(Unity.TestIgnores);
    unity_puts(" Ignored\n");
    unity_puts((Unity.TestFailures == 0U) ? "OK\n" : "FAIL\n");
    UNITY_OUTPUT_FLUSH();
    return (int)Unity.TestFailures;
}

void UnityFail(const char *message, UNITY_LINE_TYPE line)
{
    (void)unity_fail_at(message, line);
}

bool UnityAssertTrue(bool condition, const char *message, UNITY_LINE_TYPE line)
{
    return condition ? true : unity_fail_at(message, line);
}

bool UnityAssertNull(const void *pointer, bool expect_null, const char *message, UNITY_LINE_TYPE line)
{
    const bool is_null = (pointer == NULL);
    return (is_null == expect_null) ? true : unity_fail_at(message, line);
}

bool UnityAssertEqualInt(UNITY_INT expected, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line)
{
    if (expected == actual) return true;
    unity_print_test_prefix(line);
    unity_puts("FAIL: "); unity_puts(message); unity_puts(" Expected ");
    unity_print_i64(expected); unity_puts(" Was "); unity_print_i64(actual); unity_putc('\n');
    Unity.CurrentTestFailed = 1U;
    UNITY_OUTPUT_FLUSH();
    return false;
}

bool UnityAssertEqualUInt(UNITY_UINT expected, UNITY_UINT actual, const char *message, UNITY_LINE_TYPE line)
{
    if (expected == actual) return true;
    unity_print_test_prefix(line);
    unity_puts("FAIL: "); unity_puts(message); unity_puts(" Expected ");
    unity_print_u64(expected); unity_puts(" Was "); unity_print_u64(actual); unity_putc('\n');
    Unity.CurrentTestFailed = 1U;
    UNITY_OUTPUT_FLUSH();
    return false;
}

bool UnityAssertEqualFloat(UNITY_FLOAT expected, UNITY_FLOAT actual, UNITY_FLOAT delta, const char *message, UNITY_LINE_TYPE line)
{
    float difference = expected - actual;
    if (difference < 0.0f) difference = -difference;
    if (difference <= delta) return true;
    unity_print_test_prefix(line);
    unity_puts("FAIL: "); unity_puts(message); unity_puts(" Expected ");
    unity_print_float(expected); unity_puts(" +/- "); unity_print_float(delta);
    unity_puts(" Was "); unity_print_float(actual); unity_putc('\n');
    Unity.CurrentTestFailed = 1U;
    UNITY_OUTPUT_FLUSH();
    return false;
}

static bool unity_strings_equal(const char *a, const char *b)
{
    if ((a == NULL) || (b == NULL)) return a == b;
    while ((*a != '\0') && (*b != '\0')) {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

bool UnityAssertEqualString(const char *expected, const char *actual, const char *message, UNITY_LINE_TYPE line)
{
    if (unity_strings_equal(expected, actual)) return true;
    unity_print_test_prefix(line);
    unity_puts("FAIL: "); unity_puts(message); unity_puts(" Expected \"");
    unity_puts(expected); unity_puts("\" Was \""); unity_puts(actual); unity_puts("\"\n");
    Unity.CurrentTestFailed = 1U;
    UNITY_OUTPUT_FLUSH();
    return false;
}

bool UnityAssertGreater(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line)
{
    return (actual > threshold) ? true : unity_fail_at(message, line);
}

bool UnityAssertLess(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line)
{
    return (actual < threshold) ? true : unity_fail_at(message, line);
}

bool UnityAssertGreaterOrEqual(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line)
{
    return (actual >= threshold) ? true : unity_fail_at(message, line);
}

bool UnityAssertLessOrEqual(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line)
{
    return (actual <= threshold) ? true : unity_fail_at(message, line);
}
