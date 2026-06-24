/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * RGB LED engine: renders a priority arbiter onto a PWM RGB LED. The tick
 * re-arms itself only while a layer is active; every mutator kicks it so a
 * freshly-set layer animates. Pure logic is in led_effect.c / led_arbiter.c.
 */

#include <led/rgb_led.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>

/** Render tick; lower = smoother fades, more CPU. */
#define RGB_LED_TICK_MS  (40)
#define RGB_CHANNEL_MAX  (255) /* led_color is 8-bit per channel */
#define RGB_PCT_FULL     (100) /* led_set_brightness() takes a percentage */

static uint8_t to_pct(uint8_t v)
{
	return (uint8_t)((uint32_t)v * RGB_PCT_FULL / RGB_CHANNEL_MAX);
}

static void apply(struct rgb_led *led, struct led_color c)
{
	(void)led_set_brightness(led->pwm, led->ch_r, to_pct(c.r));
	(void)led_set_brightness(led->pwm, led->ch_g, to_pct(c.g));
	(void)led_set_brightness(led->pwm, led->ch_b, to_pct(c.b));
}

static void tick_fn(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct rgb_led *led = CONTAINER_OF(dwork, struct rgb_led, tick);

	k_mutex_lock(&led->lock, K_FOREVER);
	const uint32_t now = k_uptime_get_32();
	const struct led_color c = led_arbiter_render(&led->arbiter, now);
	const bool active = led_arbiter_active(&led->arbiter, now) >= 0;

	k_mutex_unlock(&led->lock);

	apply(led, c);

	if (active) {
		(void)k_work_reschedule(&led->tick, K_MSEC(RGB_LED_TICK_MS));
	}
}

static void kick(struct rgb_led *led)
{
	(void)k_work_reschedule(&led->tick, K_NO_WAIT);
}

int rgb_led_init(struct rgb_led *led)
{
	if (!device_is_ready(led->pwm)) {
		return -ENODEV;
	}

	led_arbiter_init(&led->arbiter);
	k_mutex_init(&led->lock);
	k_work_init_delayable(&led->tick, tick_fn);
	apply(led, (struct led_color){ 0, 0, 0 });
	return 0;
}

void rgb_led_set(struct rgb_led *led, uint8_t layer,
		 const struct led_effect *effect, uint32_t lifetime_ms)
{
	k_mutex_lock(&led->lock, K_FOREVER);
	led_arbiter_set(&led->arbiter, layer, effect, k_uptime_get_32(), lifetime_ms);
	k_mutex_unlock(&led->lock);
	kick(led);
}

void rgb_led_clear(struct rgb_led *led, uint8_t layer)
{
	k_mutex_lock(&led->lock, K_FOREVER);
	led_arbiter_clear(&led->arbiter, layer);
	k_mutex_unlock(&led->lock);
	kick(led);
}
