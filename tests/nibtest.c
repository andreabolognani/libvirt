#include <config.h>

#include "testutils.h"

#include "rust/rust.h"

typedef enum {
    FLAG_EXPECT_ERROR = 1 << 0,
    FLAG_EXPECT_FAILURE = 1 << 1,
} testFlags;

typedef struct {
    NibDevicesMemballoonModel model;
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
testNibDevicesMemballoon(const void *opaque)
{
    testInfo *info = (testInfo *) opaque;
    VIR_AUTOPTR(NibDevicesMemballoon) memballoon = NULL;
    VIR_AUTOFREE(char *) actual = NULL;

    memballoon = nib_devices_memballoon_new();

    nib_devices_memballoon_set_model(memballoon, info->model);
    actual = nib_devices_memballoon_to_str(memballoon);

    if (!STREQ_NULLABLE(info->expect, actual)) {
        virTestDifference(stderr, info->expect, actual);

        if (!(info->flags & FLAG_EXPECT_FAILURE))
            return -1;
    }

    return 0;
}

#define DO_TEST_BALLOON_FULL(_model, _expect, _flags) \
    do { \
        testInfo info = { \
            .model = _model, \
            .expect = _expect, \
            .flags = _flags, \
        }; \
        if (virTestRun("devices::Memballoon", \
                       testNibDevicesMemballoon, &info) < 0) { \
            ret = -1; \
        } \
    } while (0);

#define DO_TEST_BALLOON(_model, _expect) \
    DO_TEST_BALLOON_FULL(_model, _expect, 0)

static int
mymain(void)
{
    int ret = 0;

    DO_TEST_FILENAME_ERROR("no-extension", NULL);
    DO_TEST_FILENAME_FAILURE("one", "two", "three");
    DO_TEST_FILENAME("guest", ".xml", "guest.xml");

    DO_TEST_BALLOON(NIB_DEVICES_MEMBALLOON_MODEL_NONE, "");
    DO_TEST_BALLOON(NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO,
                    "virtio-memballoon");
    DO_TEST_BALLOON(NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL,
                    "virtio-memballoon-non-transitional");
    DO_TEST_BALLOON(NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL,
                    "virtio-memballoon-transitional");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIR_TEST_MAIN(mymain)
