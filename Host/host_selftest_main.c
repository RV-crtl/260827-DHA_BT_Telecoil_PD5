#include "self_test_runner.h"

#include <stdio.h>

static void host_output(const char *text)
{
    fputs(text, stdout);
}

int main(void)
{
    return SelfTest_RunAll(host_output) == 0 ? 0 : 1;
}
