/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pure LED priority arbiter — no hardware, unit-tested.
 */

#include <led/led_arbiter.h>

#include <stddef.h>
#include <string.h>

/* A slot is live if set, not past its time limit, and (for a run-once effect)
 * not yet finished. Note: time-limit compare is not wrap-safe (~49-day uptime
 * edge) — acceptable for an indicator. */
static bool slot_live(const struct led_slot *s, uint32_t now_ms)
{
	if (s->effect == NULL) {
		return false;
	}
	if (s->expire_ms != 0 && now_ms >= s->expire_ms) {
		return false;
	}

	bool done = false;

	(void)led_effect_render(s->effect, now_ms - s->start_ms, &done);
	return !done;
}

void led_arbiter_init(struct led_arbiter *a)
{
	memset(a, 0, sizeof(*a));
}

void led_arbiter_set(struct led_arbiter *a, uint8_t layer,
		     const struct led_effect *effect, uint32_t now_ms,
		     uint32_t lifetime_ms)
{
	if (layer >= LED_ARBITER_MAX_LAYERS) {
		return;
	}

	a->slots[layer].effect = effect;
	a->slots[layer].start_ms = now_ms;
	a->slots[layer].expire_ms = (lifetime_ms == 0) ? 0 : (now_ms + lifetime_ms);
}

void led_arbiter_clear(struct led_arbiter *a, uint8_t layer)
{
	if (layer < LED_ARBITER_MAX_LAYERS) {
		a->slots[layer].effect = NULL;
	}
}

int led_arbiter_active(const struct led_arbiter *a, uint32_t now_ms)
{
	for (int l = LED_ARBITER_MAX_LAYERS - 1; l >= 0; l--) {
		if (slot_live(&a->slots[l], now_ms)) {
			return l;
		}
	}

	return -1;
}

struct led_color led_arbiter_render(struct led_arbiter *a, uint32_t now_ms)
{
	int winner = -1;

	for (int l = LED_ARBITER_MAX_LAYERS - 1; l >= 0; l--) {
		struct led_slot *s = &a->slots[l];

		if (s->effect == NULL) {
			continue;
		}
		if (slot_live(s, now_ms)) {
			if (winner < 0) {
				winner = l;
			}
		} else {
			s->effect = NULL; /* retire expired / finished transient */
		}
	}

	if (winner < 0) {
		return (struct led_color){ 0, 0, 0 };
	}

	const struct led_slot *w = &a->slots[winner];

	return led_effect_render(w->effect, now_ms - w->start_ms, NULL);
}
