/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_LED_EFFECT_H_
#define NRFMODULE_LED_EFFECT_H_

/**
 * @file led_effect.h
 * @brief Pure RGB-LED effect model + renderer (no hardware).
 *
 * An effect is a sequence of steps, each ramping the colour toward a target over
 * substeps (linear interpolation = fade/breathe). substep_count<=1 means an
 * instant set held for the step. The renderer is a pure function of
 * (effect, elapsed), so it is unit-testable. Shape borrowed from Nordic CAF's
 * led_effect.
 *
 * @note The struct/macro names (led_color, led_effect, LED_EFFECT_*) match CAF's
 *       caf/led_effect.h. Do not include both in the same translation unit.
 */

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>

/** RGB colour, 0..255 per channel (PWM brightness). */
struct led_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/** One effect step. Step duration = substep_count * substep_time_ms. */
struct led_effect_step {
	struct led_color color;   /**< Target colour at the end of the step. */
	uint16_t substep_count;   /**< 1 = instant/hold; >1 = fade over the step. */
	uint16_t substep_time_ms; /**< Time per substep. */
};

/** An effect: a sequence of steps, optionally looping. */
struct led_effect {
	const struct led_effect_step *steps;
	uint16_t step_count;
	bool loop;                /**< Restart after the last step (else run once). */
};

/* ---- effect builders (CAF-style; compound-literal step arrays) ------------
 * These build static effect tables, so use them only at file scope: the step
 * arrays are compound literals, which have automatic storage inside a function.
 */

/* Braced initializer (not a compound literal) so it is a constant expression
 * usable in the static effect tables below — same shape as CAF's LED_COLOR. */
#define LED_RGB(r_, g_, b_) { .r = (r_), .g = (g_), .b = (b_) }
#define LED_BLACK           LED_RGB(0, 0, 0)

/** Substeps used by a breathe — more = smoother fade. */
#define LED_BREATHE_SUBSTEPS (24)

/** Solid colour, held. */
#define LED_EFFECT_SOLID(color_)                                             \
	{ .steps = (const struct led_effect_step[]){ { color_, 1, 1000 } },  \
	  .step_count = 1, .loop = true }

/** 50/50 on/off blink with instant edges, looping. */
#define LED_EFFECT_BLINK(period_ms_, color_)                                   \
	{ .steps = (const struct led_effect_step[]){                           \
		{ color_,  1, (period_ms_) / 2 },                            \
		{ LED_BLACK, 1, (period_ms_) / 2 } },                          \
	  .step_count = 2, .loop = true }

/** Breathe: fade up to colour then back down over the period, looping. */
#define LED_EFFECT_BREATHE(period_ms_, color_)                                 \
	{ .steps = (const struct led_effect_step[]){                           \
		{ color_,  LED_BREATHE_SUBSTEPS,                             \
		  (period_ms_) / 2 / LED_BREATHE_SUBSTEPS },                   \
		{ LED_BLACK, LED_BREATHE_SUBSTEPS,                             \
		  (period_ms_) / 2 / LED_BREATHE_SUBSTEPS } },                 \
	  .step_count = 2, .loop = true }

/** Brief flash of @p on_ms once per @p period_ms (low-duty), looping. */
#define LED_EFFECT_FLASH(on_ms_, period_ms_, color_)                           \
	{ .steps = (const struct led_effect_step[]){                           \
		{ color_,  1, (on_ms_) },                                    \
		{ LED_BLACK, 1, (period_ms_) - (on_ms_) } },                   \
	  .step_count = 2, .loop = true }

/* color arrives as a braced initializer `{ .r, .g, .b }`; its commas would be
 * counted as extra macro args, so take it variadically to re-absorb them. */
#define LED_BLINK_PAIR_(i_, on_, off_, ...)                                    \
	{ __VA_ARGS__, 1, (on_) }, { LED_BLACK, 1, (off_) }

/** @p n_ blinks then stop (run-once; the arbiter reverts to the base). */
#define LED_EFFECT_BLINK_N(n_, on_ms_, off_ms_, color_)                        \
	{ .steps = (const struct led_effect_step[]){                           \
		LISTIFY(n_, LED_BLINK_PAIR_, (,), (on_ms_), (off_ms_), color_) }, \
	  .step_count = 2 * (n_), .loop = false }

/**
 * @brief Pure: colour at @p elapsed_ms into @p e.
 *
 * Fades between steps (substep_count>1) or holds (substep_count<=1). When a
 * non-looping effect has run past its last step, returns the last colour and
 * sets *done (the arbiter uses this to retire a finished transient). For a
 * looping effect, time wraps and *done stays false.
 *
 * @param e     Effect (NULL or empty -> black).
 * @param elapsed_ms  Time since the effect started.
 * @param done  Out: true iff a non-looping effect has finished. May be NULL.
 */
struct led_color led_effect_render(const struct led_effect *e, uint32_t elapsed_ms,
				   bool *done);

/** Total duration of one pass of @p e in ms (0 if empty). */
uint32_t led_effect_duration_ms(const struct led_effect *e);

#endif /* NRFMODULE_LED_EFFECT_H_ */
