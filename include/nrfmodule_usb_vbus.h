/*
 * Copyright (c) 2026 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_USB_VBUS_H_
#define NRFMODULE_USB_VBUS_H_

/**
 * @file
 * @brief Debounced USB VBUS presence signal.
 *
 * One shared signal for the whole system. The board layer samples the raw VBUS
 * line, runs it through the debounce filter (see nrfmodule_vbus_debounce.h) and
 * publishes the result here. The board's own USBD gate and the application see
 * the same edges, so a bouncing cable cannot drive them apart.
 *
 * Boot while plugged in produces no edge. Read the level with
 * nrfmodule_usb_vbus_is_present() to learn the state at startup.
 */

#include <stdbool.h>

/**
 * @brief Debounced VBUS edge callback.
 *
 * Context: the board VBUS work item, which runs on the system workqueue.
 * Do not block: no sleeps, no long operations, no waiting on the same
 * workqueue. Defer real work to your own work item or thread.
 *
 * @param present  true on a debounced rise, false on a debounced fall
 * @param user_data  Pointer passed to nrfmodule_usb_vbus_set_callback()
 */
typedef void (*nrfmodule_usb_vbus_cb_t)(bool present, void *user_data);

/**
 * @brief Register the debounced VBUS edge callback.
 *
 * One slot. Pass NULL to clear it.
 *
 * @param cb         Callback, or NULL to unregister
 * @param user_data  Pointer handed back to the callback
 *
 * @retval 0 on success
 * @retval -EALREADY if a different callback is already registered
 */
int nrfmodule_usb_vbus_set_callback(nrfmodule_usb_vbus_cb_t cb, void *user_data);

/**
 * @brief Current debounced VBUS level.
 *
 * Safe from any context. Valid from board init onwards; before that it reads
 * false.
 *
 * @return true when a cable has been present for the debounce window.
 */
bool nrfmodule_usb_vbus_is_present(void);

/**
 * @brief Publish a debounced level. Board layer only.
 *
 * Called by board_power.c from its VBUS work item. Applications consume the
 * signal with the two functions above.
 *
 * @param present  Debounced level
 * @param edge     true when this is an edge (invoke the callback), false when
 *                 seeding the level at boot
 */
void nrfmodule_usb_vbus_publish(bool present, bool edge);

#endif /* NRFMODULE_USB_VBUS_H_ */
