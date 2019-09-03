/*
 * virtblocks_glue.h: Glue for Virt Blocks code
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "virtblocks.h"
#include "conf/domain_conf.h"

VIR_DEFINE_AUTOPTR_FUNC(VirtBlocksDevicesMemballoon,
                        virtblocks_devices_memballoon_free);

int virDomainMemballoonConvertToVirtBlocks(virDomainMemballoonDef *from,
                                           VirtBlocksDevicesMemballoon **to);
