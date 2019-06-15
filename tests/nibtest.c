#include <config.h>

#include "testutils.h"

#include "rust/rust.h"

typedef enum {
    FLAG_EXPECT_ERROR = 1 << 0,
    FLAG_EXPECT_FAILURE = 1 << 1,
} testFlags;

typedef struct {
    const char *base;
    const char *ext;
    const char *expect;
    int flags;
} testInfo;

static int
testNibUtilBuildFileName(const void *opaque)
{
    testInfo *info = (testInfo *) opaque;
    VIR_AUTOFREE(char *) actual = NULL;

    if (nib_util_build_file_name(&actual, info->base, info->ext) < 0) {
        if (!(info->flags & FLAG_EXPECT_ERROR))
            return -1;
    }

    if (!STREQ_NULLABLE(info->expect, actual)) {
        virTestDifference(stderr, info->expect, actual);

        if (!(info->flags & FLAG_EXPECT_FAILURE))
            return -1;
    }

    return 0;
}

#define DO_TEST_FILENAME_FULL(_base, _ext, _expect, _flags) \
    do { \
        testInfo info = { \
            .base = _base, \
            .ext = _ext, \
            .expect = _expect, \
            .flags = _flags, \
        }; \
        if (virTestRun("util::build_file_name()", \
                       testNibUtilBuildFileName, &info) < 0) { \
            ret = -1; \
        } \
    } while (0);

#define DO_TEST_FILENAME_ERROR(_base, _ext) \
    DO_TEST_FILENAME_FULL(_base, _ext, NULL, FLAG_EXPECT_ERROR)
#define DO_TEST_FILENAME_FAILURE(_base, _ext, _expect) \
    DO_TEST_FILENAME_FULL(_base, _ext, _expect, FLAG_EXPECT_FAILURE)
#define DO_TEST_FILENAME(_base, _ext, _expect) \
    DO_TEST_FILENAME_FULL(_base, _ext, _expect, 0)

static int
mymain(void)
{
    int ret = 0;

    DO_TEST_FILENAME_ERROR("no-extension", NULL);
    DO_TEST_FILENAME_FAILURE("one", "two", "three");
    DO_TEST_FILENAME("guest", ".xml", "guest.xml");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIR_TEST_MAIN(mymain)
