#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

/*
 * Course-aligned Unity API subset.
 *
 * This project intentionally uses the same public test syntax demonstrated in
 * the ZEIT4230 Unity tutorial: UNITY_BEGIN(), RUN_TEST(), UNITY_END(), and
 * TEST_ASSERT_* macros.  Only the assertion surface required by this project
 * is implemented, keeping the embedded test binary small and dependency-free.
 * The three files in Tests/Unity are drop-in replaceable by the upstream
 * ThrowTheSwitch Unity src/unity.c, src/unity.h and src/unity_internals.h.
 */

#include "unity_internals.h"

#ifdef __cplusplus
extern "C" {
#endif

void setUp(void);
void tearDown(void);

#define UNITY_BEGIN() (UnityBegin(__FILE__), 0)
#define UNITY_END() UnityEnd()
#define RUN_TEST(func) do { Unity.TestFile = __FILE__; UnityDefaultTestRun((func), #func, (UNITY_LINE_TYPE)__LINE__); } while (0)

#define TEST_ASSERT_TRUE(condition) \
    do { if (!UnityAssertTrue((condition) ? true : false, "Expected TRUE", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)
#define TEST_ASSERT_FALSE(condition) \
    do { if (!UnityAssertTrue(!(condition), "Expected FALSE", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)
#define TEST_ASSERT_NULL(pointer) \
    do { if (!UnityAssertNull((const void *)(pointer), true, "Expected NULL", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)
#define TEST_ASSERT_NOT_NULL(pointer) \
    do { if (!UnityAssertNull((const void *)(pointer), false, "Expected non-NULL", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { if (!UnityAssertEqualInt((UNITY_INT)(expected), (UNITY_INT)(actual), "Integer mismatch", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)
#define TEST_ASSERT_EQUAL_INT16(expected, actual) TEST_ASSERT_EQUAL_INT((expected), (actual))
#define TEST_ASSERT_EQUAL_INT32(expected, actual) TEST_ASSERT_EQUAL_INT((expected), (actual))

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    do { if (!UnityAssertEqualUInt((UNITY_UINT)(expected), (UNITY_UINT)(actual), "Unsigned mismatch", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual) TEST_ASSERT_EQUAL_UINT((expected), (actual))
#define TEST_ASSERT_EQUAL_SIZE_T(expected, actual) TEST_ASSERT_EQUAL_UINT((expected), (actual))

#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) \
    do { if (!UnityAssertEqualFloat((UNITY_FLOAT)(expected), (UNITY_FLOAT)(actual), (UNITY_FLOAT)(delta), "Float mismatch", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { if (!UnityAssertEqualString((expected), (actual), "String mismatch", (UNITY_LINE_TYPE)__LINE__)) return; } while (0)

#define TEST_ASSERT_GREATER_THAN(threshold, actual) \
    do { if (!((actual) > (threshold))) { UnityFail("Expected value greater than threshold", (UNITY_LINE_TYPE)__LINE__); return; } } while (0)
#define TEST_ASSERT_LESS_THAN(threshold, actual) \
    do { if (!((actual) < (threshold))) { UnityFail("Expected value less than threshold", (UNITY_LINE_TYPE)__LINE__); return; } } while (0)
#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual) \
    do { if (!((actual) >= (threshold))) { UnityFail("Expected value >= threshold", (UNITY_LINE_TYPE)__LINE__); return; } } while (0)
#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual) \
    do { if (!((actual) <= (threshold))) { UnityFail("Expected value <= threshold", (UNITY_LINE_TYPE)__LINE__); return; } } while (0)

#ifdef __cplusplus
}
#endif

#endif
