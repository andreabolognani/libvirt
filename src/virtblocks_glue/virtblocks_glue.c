#include <config.h>

#include "util/virerror.h"
#include "virlog.h"

#include "virtblocks_glue.h"

#define VIR_FROM_THIS VIR_FROM_VIRTBLOCKS

VIR_LOG_INIT("virtblocks");

static int
virDomainConvertToVirtBlocksDisk(virDomainDef *from,
                                 VirtBlocksDevicesDisk **to)
{
    VIR_AUTOPTR(VirtBlocksDevicesDisk) disk = NULL;
    virDomainDiskDef *fromDisk = NULL;

    if (from->ndisks != 1)
        return -1;

    fromDisk = from->disks[0];

    if (fromDisk->device != VIR_DOMAIN_DISK_DEVICE_DISK ||
        fromDisk->bus != VIR_DOMAIN_DISK_BUS_VIRTIO ||
        virDomainDiskGetType(fromDisk) != VIR_STORAGE_TYPE_FILE ||
        virDomainDiskGetFormat(fromDisk) != VIR_STORAGE_FILE_QCOW2 ||
        STRNEQ(virDomainDiskGetDriver(fromDisk), "qemu")) {
        return -1;
    }

    disk = virtblocks_devices_disk_new();
    virtblocks_devices_disk_set_filename(disk,
                                         virDomainDiskGetSource(fromDisk));

    VIR_STEAL_PTR(*to, disk);

    return 0;
}

int
virDomainConvertToVirtBlocks(virDomainDef *from,
                             VirtBlocksVmDescription **to)
{
    VIR_AUTOPTR(VirtBlocksVmDescription) vm = NULL;
    VIR_AUTOPTR(VirtBlocksDevicesDisk) disk = NULL;

    vm = virtblocks_vm_description_new();

    if (virDomainConvertToVirtBlocksDisk(from, &disk) < 0)
        goto error;

    virtblocks_vm_description_set_emulator(vm, from->emulator);
    virtblocks_vm_description_set_disk(vm, disk);

    /* These only work for the simplest of cases */
    virtblocks_vm_description_set_cpus(vm, virDomainDefGetVcpusMax(from));
    virtblocks_vm_description_set_memory(vm, virDomainDefGetMemoryInitial(from) / 1024);

    VIR_STEAL_PTR(*to, vm);

    return 0;

 error:
    virReportError(VIR_ERR_CONFIG_UNSUPPORTED,
                   "%s",
                   "Unsupported configuration");
    return -1;
}
