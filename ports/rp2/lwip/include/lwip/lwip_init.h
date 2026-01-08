// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lwip/netif.h"


void lwip_helper_init(void);

void lwip_helper_add_netif(struct netif *netif);
