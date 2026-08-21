/*
 * PigeonTracker board power management
 *
 * - Sensor power rail managed via regulator-fixed DTS node (sensor_pwr).
 *   Sensors are powered on at boot (regulator-boot-on) so I2C drivers
 *   can probe successfully. App turns sensors off when not needed.
 * - USB VBUS detection: disable USBD when unplugged to save ~1mA. Raw VBUS
 *   edges are debounced (2 s stable) and published as one shared signal,
 *   see nrfmodule_usb_vbus.h.
 *
 * Only compiled for the application (CONFIG_GPIO), not MCUboot.
 *
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>

/* ========================================================================
 * USB VBUS power management - disable USBD when cable is removed
 * ======================================================================== */

/* The gate runs on the shared debounced VBUS signal, so it needs both. */
#if defined(CONFIG_USB_DEVICE_STACK_NEXT) && defined(CONFIG_NRFMODULE_USB_VBUS)

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_power.h>
#include <nrfmodule_usb_vbus.h>
#include <nrfmodule_vbus_debounce.h>
#include <usb_vbus_internal.h>

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
 * ordering. Must run after cdc_acm_serial SYS_INIT which uses APPLICATION
 * priority 90; same priority is safe - board library links after USB. */
#define BOARD_USB_INIT_PRIORITY NRFMODULE_BOARD_USB_INIT_PRIORITY

/* Delay before the one confirming resample that follows any VBUS activity. */
#define VBUS_CONFIRM_RESAMPLE_MS (NRFMODULE_VBUS_DEBOUNCE_STABLE_MS)

static struct usbd_context *usb_ctx;
static struct k_work usb_enable_work;
static struct k_work usb_disable_work;
static struct k_work_delayable vbus_work;
static struct nrfmodule_vbus_debounce vbus_db;
/* Set by external triggers (USBD message, boot resample); the work item
 * consumes it to owe one confirming resample after the filter settles.
 */
static atomic_t vbus_confirm_owed;

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

	/* The cable can go while an app-owned enable request is queued. Enabling
	 * on a dead bus costs ~1mA and would re-arm the shell log mirror over a
	 * CDC that cannot drain.
	 */
	if (!nrfmodule_usb_vbus_is_present()) {
		LOG_DBG("Enable skipped - VBUS gone");
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

/* All debounce state lives in this work item (system workqueue), so the raw
 * level is sampled here instead of being passed in from the USBD callback.
 * The USBD gate and the public signal follow the same debounced edges.
 */
static void vbus_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	const int64_t now_ms = k_uptime_get();
	const bool raw_level = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
	const enum nrfmodule_vbus_debounce_event evt =
		nrfmodule_vbus_debounce_feed(&vbus_db, raw_level, now_ms);

	/* Publish before submitting so the enable work sees the new level. */
	switch (evt) {
	case NRFMODULE_VBUS_DEBOUNCE_EVENT_RISE:
		nrfmodule_usb_vbus_publish(true, true);
		/* App-owned mode: publish only, the app enables when it is ready. */
		if (!IS_ENABLED(CONFIG_NRFMODULE_USB_VBUS_APP_OWNED_ENABLE)) {
			(void)k_work_submit(&usb_enable_work);
		}
		break;
	case NRFMODULE_VBUS_DEBOUNCE_EVENT_FALL:
		nrfmodule_usb_vbus_publish(false, true);
		(void)k_work_submit(&usb_disable_work);
		break;
	default:
		break;
	}

	int64_t delay_ms;

	if (nrfmodule_vbus_debounce_next_timeout(&vbus_db, now_ms, &delay_ms)) {
		(void)k_work_reschedule(&vbus_work, K_MSEC(delay_ms));
	} else if (evt != NRFMODULE_VBUS_DEBOUNCE_EVENT_NONE ||
		   atomic_cas(&vbus_confirm_owed, 1, 0)) {
		/* usbd messages drop under load; end every externally
		 * triggered run with one confirming resample.
		 */
		(void)atomic_set(&vbus_confirm_owed, 0);
		(void)k_work_reschedule(&vbus_work,
					K_MSEC(VBUS_CONFIRM_RESAMPLE_MS));
	}
}

int nrfmodule_usb_vbus_enable_request(void)
{
	if (usb_ctx == NULL) {
		return -ENODEV;
	}

	if (!nrfmodule_usb_vbus_is_present()) {
		return -EAGAIN;
	}

	(void)k_work_submit(&usb_enable_work);

	return 0;
}

static void usbd_msg_cb(struct usbd_context *const ctx,
			const struct usbd_msg *const msg)
{
	ARG_UNUSED(ctx);

	switch (msg->type) {
	case USBD_MSG_VBUS_REMOVED:
	case USBD_MSG_VBUS_READY:
		/* An edge only means "resample": the work item decides. */
		(void)atomic_set(&vbus_confirm_owed, 1);
		(void)k_work_reschedule(&vbus_work, K_NO_WAIT);
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
	k_work_init_delayable(&vbus_work, vbus_work_fn);

	/* Boot produces no edge: seed the debouncer, publish the level.
	 * cdc_acm_serial already enabled USBD at boot; gate it off unless a
	 * cable is in and enable is not app-owned.
	 */
	const bool vbus_at_boot = nrf_power_usbregstatus_vbusdet_get(NRF_POWER);

	nrfmodule_vbus_debounce_init(&vbus_db, vbus_at_boot);
	nrfmodule_usb_vbus_publish(vbus_at_boot, false);

	if (!vbus_at_boot ||
	    IS_ENABLED(CONFIG_NRFMODULE_USB_VBUS_APP_OWNED_ENABLE)) {
		(void)k_work_submit(&usb_disable_work);
	} else {
		(void)k_work_submit(&usb_enable_work);
	}

	const int err = usbd_msg_register_cb(usb_ctx, usbd_msg_cb);

	if (err) {
		LOG_ERR("Failed to register USBD message callback: %d", err);
		return err;
	}

	/* A VBUS message between the seed above and the registration is lost, so
	 * resample once now that messages are being delivered.
	 */
	(void)atomic_set(&vbus_confirm_owed, 1);
	(void)k_work_reschedule(&vbus_work, K_NO_WAIT);

	LOG_INF("Board USB power management initialized");
	return 0;
}

SYS_INIT(board_usb_power_init, APPLICATION, BOARD_USB_INIT_PRIORITY);

#endif /* CONFIG_USB_DEVICE_STACK_NEXT && CONFIG_NRFMODULE_USB_VBUS */
