/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared debounced VBUS signal: level store plus the consumer callback slot.
 * The board layer publishes, applications register and read.
 */

#include <nrfmodule_usb_vbus.h>
#include <usb_vbus_internal.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/toolchain.h>
#include <errno.h>

static atomic_t vbus_present;
/* The lock keeps the cb/user_data pair coherent between a registering thread
 * and a publish on the system workqueue.
 */
static struct k_spinlock vbus_cb_lock;
static nrfmodule_usb_vbus_cb_t vbus_cb;
static void *vbus_cb_user_data;

int nrfmodule_usb_vbus_set_callback(nrfmodule_usb_vbus_cb_t cb, void *user_data)
{
	const k_spinlock_key_t key = k_spin_lock(&vbus_cb_lock);

	if (cb != NULL && vbus_cb != NULL && vbus_cb != cb) {
		k_spin_unlock(&vbus_cb_lock, key);
		return -EALREADY;
	}

	vbus_cb_user_data = user_data;
	vbus_cb = cb;
	k_spin_unlock(&vbus_cb_lock, key);

	return 0;
}

bool nrfmodule_usb_vbus_is_present(void)
{
	return atomic_get(&vbus_present) != 0;
}

void nrfmodule_usb_vbus_publish(bool present, bool edge)
{
	(void)atomic_set(&vbus_present, present ? 1 : 0);

	if (!edge) {
		return;
	}

	const k_spinlock_key_t key = k_spin_lock(&vbus_cb_lock);
	const nrfmodule_usb_vbus_cb_t cb = vbus_cb;
	void *const user_data = vbus_cb_user_data;

	k_spin_unlock(&vbus_cb_lock, key);

	if (cb != NULL) {
		cb(present, user_data);
	}
}

/* Boards that gate USBD override this in their board_power.c. The default keeps
 * the link resolved for boards that have no USB gate.
 */
__weak int nrfmodule_usb_vbus_enable_request(void)
{
	return -ENOTSUP;
}
