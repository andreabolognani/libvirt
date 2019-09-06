#include <config.h>

#include "testutils.h"

#include "virtblocks_glue/virtblocks_glue.h"

typedef enum {
    FLAG_EXPECT_ERROR = 1 << 0,
    FLAG_EXPECT_FAILURE = 1 << 1,
} testFlags;

typedef struct {
    VirtBlocksDevicesMemballoonModel model;
    const char *expect;
    unsigned int flags;
} testInfo;

static int
testVirtBlocksDevicesMemballoon(const void *opaque)
{
    testInfo *info = (testInfo *) opaque;
    VIR_AUTOPTR(VirtBlocksDevicesMemballoon) memballoon = NULL;
    VIR_AUTOFREE(char *) actual = NULL;

    memballoon = virtblocks_devices_memballoon_new();

    virtblocks_devices_memballoon_set_model(memballoon, info->model);
    actual = virtblocks_devices_memballoon_to_string(memballoon);

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
                       testVirtBlocksDevicesMemballoon, &info) < 0) { \
            ret = -1; \
        } \
    } while (0);

#define DO_TEST_BALLOON(_model, _expect) \
    DO_TEST_BALLOON_FULL(_model, _expect, 0)

static int
mymain(void)
{
    int ret = 0;

    DO_TEST_BALLOON(VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_NONE, "");
    DO_TEST_BALLOON(VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO,
                    "virtio-memballoon");
    DO_TEST_BALLOON(VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL,
                    "virtio-memballoon-non-transitional");
    DO_TEST_BALLOON(VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL,
                    "virtio-memballoon-transitional");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIR_TEST_MAIN(mymain)
