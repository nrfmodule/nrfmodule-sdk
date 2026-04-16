/*
 * LiveTracker board init - power configuration for battery operation
 *
 * - REGOUT0 set to 3.0V for high-voltage supply (one-time UICR write)
 * - USB VBUS detection: disable USBD when unplugged to save ~1mA
 *
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>

void board_early_init_hook(void)
{
	if ((nrf_power_mainregstatus_get(NRF_POWER) ==
	     NRF_POWER_MAINREGSTATUS_HIGH) &&
	    ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
	     (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		NRF_UICR->REGOUT0 =
			(NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
			(UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos);

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		NVIC_SystemReset();
	}
}

/*
 * USB VBUS power management - disable USBD when cable is removed.
 * The new USB device stack keeps USBD and HFCLK active after unplug,
 * drawing ~1mA. This listens for VBUS events and shuts it down.
 */
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)

#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board);

/* Must run after cdc_acm_serial SYS_INIT (APPLICATION default priority) */
#define BOARD_USB_INIT_PRIORITY (99)

static struct usbd_context *usb_ctx;
static struct k_work usb_enable_work;
static struct k_work usb_disable_work;

static void usb_disable_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	if (usb_ctx == NULL) {
		return;
	}

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
	} else {
		LOG_DBG("USB plugged in - USBD enabled");
	}
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

	LOG_INF("Board USB power management initialized");
	return 0;
}

SYS_INIT(board_usb_power_init, APPLICATION, BOARD_USB_INIT_PRIORITY);

#endif /* defined(CONFIG_USB_DEVICE_STACK_NEXT) */
