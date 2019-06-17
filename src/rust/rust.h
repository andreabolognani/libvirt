#pragma once

#include "conf/domain_conf.h"

#include "nib.h"

VIR_DEFINE_AUTOPTR_FUNC(NibDevicesMemballoon, nib_devices_memballoon_free);

int virDomainMemballoonConvertToNib(virDomainMemballoonDef *from,
                                    NibDevicesMemballoon **to);
