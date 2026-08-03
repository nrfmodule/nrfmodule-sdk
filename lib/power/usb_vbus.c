/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared debounced VBUS signal: level store plus the consumer callback slot.
 * The board layer publishes, applications register and read.
 */

#include <nrfmodule_usb_vbus.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>

static atomic_t vbus_present;
static nrfmodule_usb_vbus_cb_t vbus_cb;
static void *vbus_cb_user_data;

int nrfmodule_usb_vbus_set_callback(nrfmodule_usb_vbus_cb_t cb, void *user_data)
{
	if (cb != NULL && vbus_cb != NULL && vbus_cb != cb) {
		return -EALREADY;
	}

	vbus_cb_user_data = user_data;
	vbus_cb = cb;

	return 0;
}

bool nrfmodule_usb_vbus_is_present(void)
{
	return atomic_get(&vbus_present) != 0;
}

void nrfmodule_usb_vbus_publish(bool present, bool edge)
{
	(void)atomic_set(&vbus_present, present ? 1 : 0);

	if (edge && vbus_cb != NULL) {
		vbus_cb(present, vbus_cb_user_data);
	}
}
