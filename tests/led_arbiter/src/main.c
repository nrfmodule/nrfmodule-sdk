/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <led/led_arbiter.h>

#define C(r_, g_, b_) ((struct led_color){ .r = (r_), .g = (g_), .b = (b_) })

/* Test-local layer priorities (the engine treats layers as plain indices). */
enum { L_TRACK = 0, L_CHARGE = 1, L_BLE = 2, L_ERROR = 3 };

static bool eq(struct led_color a, struct led_color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

/* Solid-colour looping effects per layer. */
static const struct led_effect_step s_green[]  = { { C(0, 255, 0),   1, 1000 } };
static const struct led_effect_step s_orange[] = { { C(255, 128, 0), 1, 1000 } };
static const struct led_effect_step s_red[]    = { { C(255, 0, 0),   1, 1000 } };
static const struct led_effect e_track  = { s_green,  1, true };
static const struct led_effect e_charge = { s_orange, 1, true };
static const struct led_effect e_error  = { s_red,    1, true };

/* Blue blink once (200ms total, run-once -> retires when finished). */
static const struct led_effect_step s_ble[] = {
	{ C(0, 0, 255), 1, 100 },
	{ C(0, 0, 0),   1, 100 },
};
static const struct led_effect e_ble_once = { s_ble, 2, false };

ZTEST_SUITE(led_arbiter, NULL, NULL, NULL, NULL, NULL);

ZTEST(led_arbiter, test_empty_is_black)
{
	struct led_arbiter a;

	led_arbiter_init(&a);
	zassert_true(eq(led_arbiter_render(&a, 0), C(0, 0, 0)), "empty black");
	zassert_equal(led_arbiter_active(&a, 0), -1, "no active layer");
}

ZTEST(led_arbiter, test_priority_order)
{
	struct led_arbiter a;

	led_arbiter_init(&a);

	led_arbiter_set(&a, L_TRACK, &e_track, 0, 0);
	zassert_true(eq(led_arbiter_render(&a, 0), C(0, 255, 0)), "tracking only");

	led_arbiter_set(&a, L_CHARGE, &e_charge, 0, 0);
	zassert_true(eq(led_arbiter_render(&a, 0), C(255, 128, 0)), "charging > tracking");

	led_arbiter_set(&a, L_ERROR, &e_error, 0, 0);
	zassert_true(eq(led_arbiter_render(&a, 0), C(255, 0, 0)), "error wins all");

	led_arbiter_clear(&a, L_ERROR);
	zassert_true(eq(led_arbiter_render(&a, 0), C(255, 128, 0)), "back to charging");
}

ZTEST(led_arbiter, test_transient_retires_to_base)
{
	struct led_arbiter a;

	led_arbiter_init(&a);
	led_arbiter_set(&a, L_CHARGE, &e_charge, 0, 0);

	/* BLE blink-once overlays charging, then retires when the effect finishes. */
	led_arbiter_set(&a, L_BLE, &e_ble_once, 1000, 0);
	zassert_true(eq(led_arbiter_render(&a, 1050), C(0, 0, 255)), "BLE overlay active");
	zassert_equal(led_arbiter_active(&a, 1050), L_BLE, "BLE wins");

	/* past the 200ms blink -> retired -> reverts to charging. */
	zassert_true(eq(led_arbiter_render(&a, 1300), C(255, 128, 0)), "reverted to charging");
	zassert_equal(led_arbiter_active(&a, 1300), L_CHARGE, "BLE retired");
}

ZTEST(led_arbiter, test_time_auto_off)
{
	struct led_arbiter a;

	led_arbiter_init(&a);
	led_arbiter_set(&a, L_TRACK, &e_track, 2000, 500);

	zassert_true(eq(led_arbiter_render(&a, 2200), C(0, 255, 0)), "tracking within window");
	zassert_true(eq(led_arbiter_render(&a, 2600), C(0, 0, 0)), "auto-off after lifetime");
	zassert_equal(led_arbiter_active(&a, 2600), -1, "retired");
}

ZTEST(led_arbiter, test_clear)
{
	struct led_arbiter a;

	led_arbiter_init(&a);
	led_arbiter_set(&a, L_TRACK, &e_track, 0, 0);
	zassert_true(eq(led_arbiter_render(&a, 0), C(0, 255, 0)), "set");
	led_arbiter_clear(&a, L_TRACK);
	zassert_true(eq(led_arbiter_render(&a, 0), C(0, 0, 0)), "cleared");
}
