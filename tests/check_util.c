#include "check_util.h"

#include <stdio.h>

static int g_fail;

void check_expect(int ok, const char *msg)
{
    if (!ok) {
        fprintf(stderr, "fail: %s\n", msg);
        g_fail = 1;
    }
}

int check_near(float a, float b, float tol)
{
    float d;

    d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d <= tol;
}

int check_failed(void)
{
    return g_fail;
}
