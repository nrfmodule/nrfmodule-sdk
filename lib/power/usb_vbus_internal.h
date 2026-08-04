/*
 * Copyright (c) 2026 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_USB_VBUS_INTERNAL_H_
#define NRFMODULE_USB_VBUS_INTERNAL_H_

/**
 * @file
 * @brief Producer side of the shared VBUS signal. Board layer only.
 *
 * Applications use nrfmodule_usb_vbus.h.
 */

#include <stdbool.h>

/**
 * @brief Publish a debounced level.
 *
 * Called by board_power.c from its VBUS work item.
 *
 * @param present  Debounced level
 * @param edge     true when this is an edge (invoke the callback), false when
 *                 seeding the level at boot
 */
void nrfmodule_usb_vbus_publish(bool present, bool edge);

#endif /* NRFMODULE_USB_VBUS_INTERNAL_H_ */
