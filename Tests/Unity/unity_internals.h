#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef UNITY_INCLUDE_CONFIG_H
#include "unity_config.h"
#endif

#ifndef UNITY_OUTPUT_CHAR
int UnityOutputChar(int c);
#define UNITY_OUTPUT_CHAR(c) UnityOutputChar((c))
#endif

#ifndef UNITY_OUTPUT_FLUSH
#define UNITY_OUTPUT_FLUSH() ((void)0)
#endif

#ifndef UNITY_LINE_TYPE
#define UNITY_LINE_TYPE unsigned int
#endif

typedef int64_t UNITY_INT;
typedef uint64_t UNITY_UINT;
typedef float UNITY_FLOAT;

typedef struct {
    const char *TestFile;
    const char *CurrentTestName;
    UNITY_LINE_TYPE CurrentTestLineNumber;
    uint32_t NumberOfTests;
    uint32_t TestFailures;
    uint32_t TestIgnores;
    uint8_t CurrentTestFailed;
    uint8_t CurrentTestIgnored;
} UNITY_STORAGE_T;

extern UNITY_STORAGE_T Unity;

void UnityBegin(const char *filename);
int UnityEnd(void);
void UnityDefaultTestRun(void (*func)(void), const char *name, UNITY_LINE_TYPE line);
void UnityFail(const char *message, UNITY_LINE_TYPE line);

bool UnityAssertEqualInt(UNITY_INT expected, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertEqualUInt(UNITY_UINT expected, UNITY_UINT actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertEqualFloat(UNITY_FLOAT expected, UNITY_FLOAT actual, UNITY_FLOAT delta, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertEqualString(const char *expected, const char *actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertTrue(bool condition, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertNull(const void *pointer, bool expect_null, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertGreater(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertLess(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertGreaterOrEqual(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line);
bool UnityAssertLessOrEqual(UNITY_INT threshold, UNITY_INT actual, const char *message, UNITY_LINE_TYPE line);

#endif
