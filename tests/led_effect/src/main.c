/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <stdlib.h>
#include <led/led_effect.h>

#define C(r_, g_, b_) ((struct led_color){ .r = (r_), .g = (g_), .b = (b_) })

static bool color_eq(struct led_color a, struct led_color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

/* within tolerance per channel (fades are interpolated) */
static bool color_near(struct led_color a, struct led_color b, int tol)
{
	return (abs((int)a.r - b.r) <= tol) && (abs((int)a.g - b.g) <= tol) &&
	       (abs((int)a.b - b.b) <= tol);
}

/* Macro-built effects must live at file scope: inside a function the compound
 * literals would be automatic, not the static storage these macros assume. */
static const struct led_effect m_blink   = LED_EFFECT_BLINK(200, LED_RGB(255, 0, 0));
static const struct led_effect m_blink_n = LED_EFFECT_BLINK_N(3, 100, 100, LED_RGB(0, 0, 255));
static const struct led_effect m_solid   = LED_EFFECT_SOLID(LED_RGB(0, 255, 0));
static const struct led_effect m_flash   = LED_EFFECT_FLASH(80, 2000, LED_RGB(0, 0, 255));

ZTEST_SUITE(led_effect, NULL, NULL, NULL, NULL, NULL);

ZTEST(led_effect, test_empty_is_black)
{
	bool done = true;
	struct led_color c = led_effect_render(NULL, 0, &done);

	zassert_true(color_eq(c, C(0, 0, 0)), "NULL effect is black");
	zassert_false(done, "NULL effect not done");

	struct led_effect empty = { .steps = NULL, .step_count = 0, .loop = true };

	c = led_effect_render(&empty, 100, &done);
	zassert_true(color_eq(c, C(0, 0, 0)), "empty effect is black");
}

ZTEST(led_effect, test_solid_holds_colour)
{
	static const struct led_effect_step steps[] = {
		{ .color = C(0, 255, 0), .substep_count = 1, .substep_time_ms = 1000 },
	};
	struct led_effect solid = { steps, 1, true };
	bool done = false;

	for (uint32_t t = 0; t < 5000; t += 333) {
		struct led_color c = led_effect_render(&solid, t, &done);

		zassert_true(color_eq(c, C(0, 255, 0)), "solid green at t=%u", t);
		zassert_false(done, "looping solid never done");
	}
}

ZTEST(led_effect, test_instant_blink)
{
	/* red 100ms, off 100ms, loop — instant (substep_count=1). */
	static const struct led_effect_step steps[] = {
		{ .color = C(255, 0, 0), .substep_count = 1, .substep_time_ms = 100 },
		{ .color = C(0, 0, 0),   .substep_count = 1, .substep_time_ms = 100 },
	};
	struct led_effect blink = { steps, 2, true };

	zassert_true(color_eq(led_effect_render(&blink, 0, NULL),   C(255, 0, 0)), "on at 0");
	zassert_true(color_eq(led_effect_render(&blink, 50, NULL),  C(255, 0, 0)), "on at 50");
	zassert_true(color_eq(led_effect_render(&blink, 100, NULL), C(0, 0, 0)),   "off at 100");
	zassert_true(color_eq(led_effect_render(&blink, 150, NULL), C(0, 0, 0)),   "off at 150");
	zassert_true(color_eq(led_effect_render(&blink, 200, NULL), C(255, 0, 0)), "wrap on at 200");
}

ZTEST(led_effect, test_fade_interpolates)
{
	/* fade black->white over 100ms, then white->black over 100ms, loop. */
	static const struct led_effect_step steps[] = {
		{ .color = C(255, 255, 255), .substep_count = 10, .substep_time_ms = 10 },
		{ .color = C(0, 0, 0),       .substep_count = 10, .substep_time_ms = 10 },
	};
	struct led_effect fade = { steps, 2, true };

	zassert_true(color_near(led_effect_render(&fade, 0, NULL),   C(0, 0, 0), 20),    "start ~black");
	zassert_true(color_near(led_effect_render(&fade, 50, NULL),  C(127, 127, 127), 30), "mid up ~half");
	zassert_true(color_near(led_effect_render(&fade, 100, NULL), C(255, 255, 255), 20), "peak ~white");
	zassert_true(color_near(led_effect_render(&fade, 150, NULL), C(127, 127, 127), 30), "mid down ~half");
}

ZTEST(led_effect, test_run_once_sets_done)
{
	/* blue blink x1 (on, off), no loop — done after total. */
	static const struct led_effect_step steps[] = {
		{ .color = C(0, 0, 255), .substep_count = 1, .substep_time_ms = 100 },
		{ .color = C(0, 0, 0),   .substep_count = 1, .substep_time_ms = 100 },
	};
	struct led_effect once = { steps, 2, false };
	bool done = false;

	struct led_color c = led_effect_render(&once, 50, &done);

	zassert_true(color_eq(c, C(0, 0, 255)), "blue mid-run");
	zassert_false(done, "not done mid-run");

	c = led_effect_render(&once, 250, &done);
	zassert_true(done, "done past total");
	zassert_true(color_eq(c, C(0, 0, 0)), "holds last colour when done");
}

ZTEST(led_effect, test_macro_blink)
{
	zassert_equal(m_blink.step_count, 2, "blink = 2 steps");
	zassert_true(m_blink.loop, "looping");
	zassert_true(color_eq(led_effect_render(&m_blink, 0, NULL), C(255, 0, 0)), "on");
	zassert_true(color_eq(led_effect_render(&m_blink, 100, NULL), C(0, 0, 0)), "off");
}

ZTEST(led_effect, test_macro_blink_n_run_once)
{
	bool done = false;

	zassert_equal(m_blink_n.step_count, 6, "3 blinks = 6 steps");
	zassert_false(m_blink_n.loop, "run-once");
	zassert_true(color_eq(led_effect_render(&m_blink_n, 0, &done), C(0, 0, 255)),
		     "first blink on");
	zassert_false(done, "not done at start");

	(void)led_effect_render(&m_blink_n, 600, &done); /* total = 6 * 100 */
	zassert_true(done, "done after 3 blinks");
}

ZTEST(led_effect, test_macro_solid_and_flash)
{
	zassert_true(color_eq(led_effect_render(&m_solid, 1234, NULL), C(0, 255, 0)), "solid");
	zassert_true(color_eq(led_effect_render(&m_flash, 40, NULL), C(0, 0, 255)), "flash on");
	zassert_true(color_eq(led_effect_render(&m_flash, 500, NULL), C(0, 0, 0)), "flash off");
}

ZTEST(led_effect, test_duration)
{
	static const struct led_effect_step steps[] = {
		{ .color = C(1, 1, 1), .substep_count = 3, .substep_time_ms = 100 },
		{ .color = C(0, 0, 0), .substep_count = 1, .substep_time_ms = 200 },
	};
	struct led_effect e = { steps, 2, true };

	zassert_equal(led_effect_duration_ms(&e), 3 * 100 + 1 * 200, "total duration");
	zassert_equal(led_effect_duration_ms(NULL), 0, "NULL duration 0");
}
