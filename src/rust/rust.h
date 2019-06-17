#pragma once

#include "virtblocks.h"
#include "conf/domain_conf.h"

VIR_DEFINE_AUTOPTR_FUNC(VirtBlocksDevicesMemballoon, virtblocks_devices_memballoon_free);

int virDomainMemballoonConvertToVirtBlocks(virDomainMemballoonDef *from,
                                           VirtBlocksDevicesMemballoon **to);
