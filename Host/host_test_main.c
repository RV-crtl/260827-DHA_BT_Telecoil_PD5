#include "test_runner.h"

#include <stdio.h>

static void host_output(const char *text)
{
    fputs(text, stdout);
}

int main(void)
{
    return RunAllTests(host_output);
}
