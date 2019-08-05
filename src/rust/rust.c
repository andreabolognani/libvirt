#include <config.h>

#include "util/virerror.h"
#include "virlog.h"

#include "rust.h"

#define VIR_FROM_THIS VIR_FROM_RUST

VIR_LOG_INIT("rust");

static VirtBlocksDevicesMemballoonModel modelTable[] = {
    VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO */
    VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_NONE, /* VIR_DOMAIN_MEMBALLOON_MODEL_XEN */
    VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_NONE, /* VIR_DOMAIN_MEMBALLOON_MODEL_NONE */
    VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL */
    VIRTBLOCKS_DEVICES_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL */
};
verify(ARRAY_CARDINALITY(modelTable) == VIR_DOMAIN_MEMBALLOON_MODEL_LAST);

int
virDomainMemballoonConvertToVirtBlocks(virDomainMemballoonDef *from,
                                       VirtBlocksDevicesMemballoon **to)
{
    VIR_AUTOPTR(VirtBlocksDevicesMemballoon) memballoon = NULL;

    memballoon = virtblocks_devices_memballoon_new();

    switch ((virDomainMemballoonModel) from->model) {
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO:
        case VIR_DOMAIN_MEMBALLOON_MODEL_NONE:
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL:
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL:
            virtblocks_devices_memballoon_set_model(memballoon,
                                                    modelTable[from->model]);
            break;

        case VIR_DOMAIN_MEMBALLOON_MODEL_XEN:
        case VIR_DOMAIN_MEMBALLOON_MODEL_LAST:
        default:
            virReportEnumRangeError(virDomainMemballoonModel,
                                    from->model);
            return -1;
    }

    VIR_STEAL_PTR(*to, memballoon);

    return 0;
}
