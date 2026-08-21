/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * BeeScales BT USB VBUS power management.
 * Disables USBD when cable is removed to save ~1mA.
 * Only compiled for the application (CONFIG_GPIO), not MCUboot.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_power.h>
#include <nrfmodule_usb_vbus.h>

LOG_MODULE_REGISTER(board);

#if defined(CONFIG_SHELL_LOG_BACKEND)
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>

/* Shell instance is shell_uart; its log backend is <instance>_backend. */
#define SHELL_UART_LOG_BACKEND_NAME "shell_uart_backend"

/* The shell's log-mirror backend shares the CDC-ACM UART transport. When the
 * USB gate disables USBD, CDC TX dies and z_shell_write() parks forever, so
 * every log message stalls the lone log thread for the shell backend's queue
 * timeout (log_core runs backends serially), starving log_backend_fs. Gate the
 * shell log backend in lockstep with USBD.
 *
 * Latch: the board only re-activates a backend it itself deactivated. This keeps
 * the boot enable-seed from activating the backend before the shell thread has
 * run shell_start() - until then cb->ctx is NULL and process() would deref it.
 */
static bool board_gated;

static void shell_log_backend_set_active(bool active)
{
	const struct log_backend *const shell_log =
		log_backend_get_by_name(SHELL_UART_LOG_BACKEND_NAME);

	if (shell_log == NULL) {
		return;
	}

	if (!active) {
		board_gated = true;
		if (log_backend_is_active(shell_log)) {
			log_backend_deactivate(shell_log);
		}
		return;
	}

	/* Re-activate only a backend the board gated, and never with a NULL ctx. */
	if (board_gated && shell_log->cb->ctx != NULL &&
	    !log_backend_is_active(shell_log)) {
		log_backend_activate(shell_log, shell_log->cb->ctx);
	}
	board_gated = false;
}
#endif /* defined(CONFIG_SHELL_LOG_BACKEND) */

/* Shared with apps via nrfmodule_usb_vbus.h so they can BUILD_ASSERT their
 * ordering against the board's USB init. */
#define BOARD_USB_INIT_PRIORITY NRFMODULE_BOARD_USB_INIT_PRIORITY

static struct usbd_context *usb_ctx;
static struct k_work usb_enable_work;
static struct k_work usb_disable_work;

static void usb_disable_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (usb_ctx == NULL) {
		return;
	}

#if defined(CONFIG_SHELL_LOG_BACKEND)
	/* Deactivate first so no message races into the about-to-wedge backend. */
	shell_log_backend_set_active(false);
#endif

	int err = usbd_disable(usb_ctx);

	if (err && err != -EALREADY) {
		LOG_ERR("usbd_disable failed: %d", err);
	} else {
		LOG_DBG("USB unplugged - USBD disabled");
	}
}

static void usb_enable_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (usb_ctx == NULL) {
		return;
	}

	int err = usbd_enable(usb_ctx);

	if (err && err != -EALREADY) {
		LOG_ERR("usbd_enable failed: %d", err);
		return;
	}

	LOG_DBG("USB plugged in - USBD enabled");

#if defined(CONFIG_SHELL_LOG_BACKEND)
	/* CDC TX is live again - re-arm the shell log mirror. */
	shell_log_backend_set_active(true);
#endif
}

static void usbd_msg_cb(struct usbd_context *const ctx,
			const struct usbd_msg *const msg)
{
	switch (msg->type) {
	case USBD_MSG_VBUS_REMOVED:
		k_work_submit(&usb_disable_work);
		break;
	case USBD_MSG_VBUS_READY:
		k_work_submit(&usb_enable_work);
		break;
	default:
		break;
	}
}

static int board_usb_power_init(void)
{
	STRUCT_SECTION_FOREACH(usbd_context, entry) {
		usb_ctx = entry;
		break;
	}

	if (usb_ctx == NULL) {
		LOG_WRN("No USBD context found");
		return -ENODEV;
	}

	k_work_init(&usb_enable_work, usb_enable_work_fn);
	k_work_init(&usb_disable_work, usb_disable_work_fn);

	int err = usbd_msg_register_cb(usb_ctx, usbd_msg_cb);

	if (err) {
		LOG_ERR("Failed to register USBD message callback: %d", err);
		return err;
	}

	/* VBUS edges are seen only as transitions; seed the current level at boot.
	 * cdc_acm_serial enables USBD at boot (ENABLE_AT_BOOT=y), so with no cable
	 * there is no VBUS-removed edge - gate it off here to drop the HFXO request
	 * and deactivate the shell log backend over the dead CDC.
	 */
	if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
		k_work_submit(&usb_enable_work);
	} else {
		k_work_submit(&usb_disable_work);
	}

	LOG_INF("Board USB power management initialized");
	return 0;
}

SYS_INIT(board_usb_power_init, APPLICATION, BOARD_USB_INIT_PRIORITY);

#endif /* defined(CONFIG_USB_DEVICE_STACK_NEXT) */
