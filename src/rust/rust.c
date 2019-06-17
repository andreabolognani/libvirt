#include "config.h"

#include "util/virerror.h"
#include "virlog.h"

#include "rust.h"

#define VIR_FROM_THIS VIR_FROM_RUST

VIR_LOG_INIT("rust");

static NibDevicesMemballoonModel modelTable[] = {
    NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO */
    NIB_DEVICES_MEMBALLOON_MODEL_NONE, /* VIR_DOMAIN_MEMBALLOON_MODEL_XEN */
    NIB_DEVICES_MEMBALLOON_MODEL_NONE, /* VIR_DOMAIN_MEMBALLOON_MODEL_NONE */
    NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL */
    NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL, /* VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL */
};
verify(ARRAY_CARDINALITY(modelTable) == VIR_DOMAIN_MEMBALLOON_MODEL_LAST);

int
virDomainMemballoonConvertToNib(virDomainMemballoonDef *from,
                                NibDevicesMemballoon **to)
{
    VIR_AUTOPTR(NibDevicesMemballoon) memballoon = NULL;

    memballoon = nib_devices_memballoon_new();

    switch ((virDomainMemballoonModel) from->model) {
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO:
        case VIR_DOMAIN_MEMBALLOON_MODEL_NONE:
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL:
        case VIR_DOMAIN_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL:
            nib_devices_memballoon_set_model(memballoon,
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
