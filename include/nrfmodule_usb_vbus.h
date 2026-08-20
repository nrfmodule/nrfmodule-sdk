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
 * @brief SYS_INIT priority (APPLICATION level) of the board layer's USB power
 *        init, which enables USBD and registers the VBUS callback.
 *
 * An app module whose USBD context must exist before that init runs uses a
 * lower priority and BUILD_ASSERTs its ordering against this symbol, so a
 * renumber here breaks that build instead of USB dying silently.
 * Bare integer: SYS_INIT stringifies the priority into the linker section
 * name (STRINGIFY, so this indirection still expands); brackets would emit
 * a "P_(90)_" section and trip the linker's initlevel_error assert.
 */
#define NRFMODULE_BOARD_USB_INIT_PRIORITY 90 /* style:no-paren, SYS_INIT stringifies the priority */

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
 * One slot, single owner: one module per image registers, then keeps it. Pass
 * NULL to clear it. Callable from any context; registration is atomic against
 * a concurrent edge publish.
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
 * @brief Ask the board to enable USBD now.
 *
 * Only meaningful with CONFIG_NRFMODULE_USB_VBUS_APP_OWNED_ENABLE=y, where the
 * board publishes the debounced rise but leaves USBD off so the application can
 * finish preparing (for example refreshing the mass-storage FAT image) before
 * the host sees the device. Without that option the board has already enabled
 * USBD and the queued enable finds nothing to do.
 *
 * The enable is refused while VBUS is absent, so a cable pulled during the
 * application's preparation cannot leave USBD powered on a dead bus.
 *
 * Callable from any thread. The enable itself runs in the board work item.
 *
 * @retval 0 when the enable was queued
 * @retval -EAGAIN if VBUS is not present, so nothing was queued
 * @retval -ENODEV if the board found no USBD context
 * @retval -ENOTSUP if the board does not implement a USB gate
 */
int nrfmodule_usb_vbus_enable_request(void);

#endif /* NRFMODULE_USB_VBUS_H_ */
