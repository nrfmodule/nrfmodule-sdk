/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_RGB_LED_H_
#define NRFMODULE_RGB_LED_H_

/**
 * @file rgb_led.h
 * @brief One PWM RGB LED = priority arbiter rendered onto three PWM channels.
 *
 * The engine: it owns an arbiter, the PWM seam, a self-arming tick, and a lock.
 * All state is per-instance, so N physical LEDs = N rgb_led objects. The pure,
 * unit-tested logic lives in led_effect / led_arbiter; this is the hardware
 * seam. Effect tables and signal→layer mapping are the caller's policy.
 *
 * Backend: a `pwm-leds` (CONFIG_LED_PWM) node with three channels. The product
 * supplies the device and the channel indices.
 */

#include <led/led_arbiter.h>
#include <led/led_effect.h>

#include <zephyr/kernel.h>

struct device;

struct rgb_led {
	const struct device *pwm; /**< a pwm-leds node */
	uint8_t ch_r;             /**< channel index of red within the node */
	uint8_t ch_g;
	uint8_t ch_b;
	struct led_arbiter arbiter;
	struct k_work_delayable tick;
	struct k_mutex lock;
};

/** Ready the device, clear all layers, blank the LED. */
int rgb_led_init(struct rgb_led *led);

/**
 * @brief Drive @p layer with @p effect from now.
 * @param layer       Priority index (higher wins); see led_arbiter.h.
 * @param lifetime_ms 0 = until cleared (or, for a run-once effect, until it
 *                    finishes); else auto-retire after this many ms.
 */
void rgb_led_set(struct rgb_led *led, uint8_t layer,
		 const struct led_effect *effect, uint32_t lifetime_ms);

/** Drop @p layer (reverts to the next live one). */
void rgb_led_clear(struct rgb_led *led, uint8_t layer);

#endif /* NRFMODULE_RGB_LED_H_ */
