#include <config.h>

#include "testutils.h"

static int
mymain(void)
{
    int ret = 0;

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIR_TEST_MAIN(mymain)
