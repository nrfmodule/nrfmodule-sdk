/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pure LED effect renderer — no hardware, unit-tested.
 */

#include <led/led_effect.h>

#include <stddef.h>

static uint8_t lerp8(uint8_t a, uint8_t b, uint32_t num, uint32_t den)
{
	/* Fixed-width: delta*num reaches ~127500, past INT16_MAX on a 16-bit int. */
	int32_t delta = (int32_t)b - (int32_t)a;

	return (uint8_t)((int32_t)a + delta * (int32_t)num / (int32_t)den);
}

static struct led_color lerp_color(struct led_color a, struct led_color b,
				   uint32_t num, uint32_t den)
{
	return (struct led_color){
		.r = lerp8(a.r, b.r, num, den),
		.g = lerp8(a.g, b.g, num, den),
		.b = lerp8(a.b, b.b, num, den),
	};
}

uint32_t led_effect_duration_ms(const struct led_effect *e)
{
	if (e == NULL || e->steps == NULL || e->step_count == 0) {
		return 0;
	}

	uint32_t total = 0;

	for (uint16_t i = 0; i < e->step_count; i++) {
		total += (uint32_t)e->steps[i].substep_count * e->steps[i].substep_time_ms;
	}

	return total;
}

struct led_color led_effect_render(const struct led_effect *e, uint32_t elapsed_ms,
				   bool *done)
{
	const struct led_color black = { 0, 0, 0 };

	if (done != NULL) {
		*done = false;
	}

	if (e == NULL || e->steps == NULL || e->step_count == 0) {
		return black;
	}

	const uint32_t total = led_effect_duration_ms(e);

	if (total == 0) {
		return e->steps[e->step_count - 1].color;
	}

	if (elapsed_ms >= total) {
		if (e->loop) {
			elapsed_ms %= total;
		} else {
			if (done != NULL) {
				*done = true;
			}
			return e->steps[e->step_count - 1].color;
		}
	}

	uint32_t acc = 0;

	for (uint16_t i = 0; i < e->step_count; i++) {
		const uint32_t dur =
			(uint32_t)e->steps[i].substep_count * e->steps[i].substep_time_ms;

		if (dur == 0) {
			continue;
		}
		if (elapsed_ms < acc + dur) {
			if (e->steps[i].substep_count <= 1) {
				return e->steps[i].color; /* instant / hold */
			}

			const uint16_t prev = (i == 0) ? (e->step_count - 1) : (i - 1);

			return lerp_color(e->steps[prev].color, e->steps[i].color,
					  elapsed_ms - acc, dur);
		}
		acc += dur;
	}

	return e->steps[e->step_count - 1].color;
}
