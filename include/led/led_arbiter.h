/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_LED_ARBITER_H_
#define NRFMODULE_LED_ARBITER_H_

/**
 * @file led_arbiter.h
 * @brief Pure priority arbiter for LED effects (no hardware).
 *
 * Each source owns a layer; the arbiter shows the highest-priority active layer.
 * Layers are plain priority indices (0 = lowest, higher wins) — the application
 * defines what each layer means (e.g. an enum mapping charging/BLE/error onto
 * indices). Transient layers retire automatically — by a time limit, or when a
 * run-once effect finishes — so e.g. a BLE-connect blink reverts to the
 * underlying base effect. Pure + unit-testable: time is passed in.
 */

#include <led/led_effect.h>

/* Number of priority layers (see CONFIG_NRFMODULE_LED_ARBITER_MAX_LAYERS). It
 * sizes struct led_arbiter, so it comes from Kconfig — one value build-wide. */
#define LED_ARBITER_MAX_LAYERS CONFIG_NRFMODULE_LED_ARBITER_MAX_LAYERS

struct led_slot {
	const struct led_effect *effect; /**< NULL = inactive. */
	uint32_t start_ms;
	uint32_t expire_ms;              /**< 0 = no time limit. */
};

struct led_arbiter {
	struct led_slot slots[LED_ARBITER_MAX_LAYERS];
};

/** Clear all layers. */
void led_arbiter_init(struct led_arbiter *a);

/**
 * @brief Activate @p layer with @p effect starting now.
 * @param layer        Priority index (0..LED_ARBITER_MAX_LAYERS-1); higher wins.
 * @param lifetime_ms  0 = until cleared (or, for a run-once effect, until it
 *                     finishes); else retire after this many ms.
 */
void led_arbiter_set(struct led_arbiter *a, uint8_t layer,
		     const struct led_effect *effect, uint32_t now_ms,
		     uint32_t lifetime_ms);

/** Deactivate @p layer. */
void led_arbiter_clear(struct led_arbiter *a, uint8_t layer);

/**
 * @brief Render the winning layer at @p now_ms, retiring expired/finished
 *        transient layers as a side effect.
 */
struct led_color led_arbiter_render(struct led_arbiter *a, uint32_t now_ms);

/** The winning layer index at @p now_ms, or -1 if none (no mutation). */
int led_arbiter_active(const struct led_arbiter *a, uint32_t now_ms);

#endif /* NRFMODULE_LED_ARBITER_H_ */
