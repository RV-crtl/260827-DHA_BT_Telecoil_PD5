#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/* Output is supplied by Tests/test_runner.c so the same tests run on the
 * NUCLEO-F446RE serial console and on the host PC. */
int UnityOutputChar(int c);
void UnityOutputFlush(void);
#define UNITY_OUTPUT_CHAR(c) UnityOutputChar((c))
#define UNITY_OUTPUT_FLUSH() UnityOutputFlush()

#endif
