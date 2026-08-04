/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <nrfmodule_vbus_debounce.h>

#define STABLE_MS NRFMODULE_VBUS_DEBOUNCE_STABLE_MS
#define EV_NONE   NRFMODULE_VBUS_DEBOUNCE_EVENT_NONE
#define EV_RISE   NRFMODULE_VBUS_DEBOUNCE_EVENT_RISE
#define EV_FALL   NRFMODULE_VBUS_DEBOUNCE_EVENT_FALL

/* Sentinel to prove next_timeout leaves delay untouched when idle. */
#define DELAY_SENTINEL (-777)

ZTEST_SUITE(vbus_debounce, NULL, NULL, NULL, NULL, NULL);

ZTEST(vbus_debounce, test_init_seeds_level_without_event)
{
	struct nrfmodule_vbus_debounce db;
	int64_t delay = DELAY_SENTINEL;

	nrfmodule_vbus_debounce_init(&db, false);
	zassert_false(nrfmodule_vbus_debounce_level(&db), "seed low");
	zassert_false(nrfmodule_vbus_debounce_next_timeout(&db, 0, &delay),
		      "no tick pending after init");
	zassert_equal(delay, DELAY_SENTINEL, "delay untouched when idle");

	nrfmodule_vbus_debounce_init(&db, true);
	zassert_true(nrfmodule_vbus_debounce_level(&db), "seed high");
}

ZTEST(vbus_debounce, test_rise_after_stable_window)
{
	struct nrfmodule_vbus_debounce db;
	int64_t delay = 0;

	nrfmodule_vbus_debounce_init(&db, false);

	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, 0), EV_NONE,
		      "no event before the window");
	zassert_true(nrfmodule_vbus_debounce_next_timeout(&db, 0, &delay),
		      "tick pending during the window");
	zassert_equal(delay, STABLE_MS, "full window from the first sample");

	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, STABLE_MS - 1),
		      EV_NONE, "still inside the window");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, STABLE_MS),
		      EV_RISE, "settles at the deadline");
	zassert_true(nrfmodule_vbus_debounce_level(&db), "level follows");
	zassert_false(nrfmodule_vbus_debounce_next_timeout(&db, STABLE_MS,
							   &delay),
		      "settled filter is idle");
}

ZTEST(vbus_debounce, test_fall_after_stable_window)
{
	struct nrfmodule_vbus_debounce db;

	nrfmodule_vbus_debounce_init(&db, true);

	zassert_equal(nrfmodule_vbus_debounce_feed(&db, false, 100), EV_NONE,
		      "no event before the window");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, false,
						   100 + STABLE_MS),
		      EV_FALL, "settles at the deadline");
	zassert_false(nrfmodule_vbus_debounce_level(&db), "level follows");
}

ZTEST(vbus_debounce, test_bounce_back_cancels_pending)
{
	struct nrfmodule_vbus_debounce db;
	int64_t delay = DELAY_SENTINEL;

	nrfmodule_vbus_debounce_init(&db, false);

	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, 0), EV_NONE,
		      "window opens");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, false, 100), EV_NONE,
		      "bounce home cancels");
	zassert_false(nrfmodule_vbus_debounce_next_timeout(&db, 100, &delay),
		      "no tick after the cancel");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, false,
						   STABLE_MS + 100),
		      EV_NONE, "cancelled change never settles");
	zassert_false(nrfmodule_vbus_debounce_level(&db), "level unchanged");
}

ZTEST(vbus_debounce, test_unchanged_sample_does_not_restart_window)
{
	struct nrfmodule_vbus_debounce db;
	int64_t delay = 0;

	nrfmodule_vbus_debounce_init(&db, false);

	(void)nrfmodule_vbus_debounce_feed(&db, true, 0);
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, 1500), EV_NONE,
		      "resample mid-window");
	zassert_true(nrfmodule_vbus_debounce_next_timeout(&db, 1500, &delay),
		      "tick still pending");
	zassert_equal(delay, STABLE_MS - 1500,
		      "deadline anchored to the first sample");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, STABLE_MS),
		      EV_RISE, "original deadline holds");
}

ZTEST(vbus_debounce, test_retrigger_restarts_window)
{
	struct nrfmodule_vbus_debounce db;

	nrfmodule_vbus_debounce_init(&db, false);

	(void)nrfmodule_vbus_debounce_feed(&db, true, 0);
	(void)nrfmodule_vbus_debounce_feed(&db, false, 500);
	(void)nrfmodule_vbus_debounce_feed(&db, true, 1000);
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true,
						   1000 + STABLE_MS - 1),
		      EV_NONE, "new window from the retrigger");
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true,
						   1000 + STABLE_MS),
		      EV_RISE, "settles on the new deadline");
}

ZTEST(vbus_debounce, test_tick_settles_without_sample)
{
	struct nrfmodule_vbus_debounce db;

	nrfmodule_vbus_debounce_init(&db, false);

	(void)nrfmodule_vbus_debounce_feed(&db, true, 0);
	zassert_equal(nrfmodule_vbus_debounce_tick(&db, STABLE_MS - 1),
		      EV_NONE, "tick before the deadline");
	zassert_equal(nrfmodule_vbus_debounce_tick(&db, STABLE_MS), EV_RISE,
		      "tick at the deadline settles");
	zassert_true(nrfmodule_vbus_debounce_level(&db), "level follows");
	zassert_equal(nrfmodule_vbus_debounce_tick(&db, 2 * STABLE_MS),
		      EV_NONE, "settled tick is a no-op");
}

ZTEST(vbus_debounce, test_next_timeout_clamps_overdue_to_zero)
{
	struct nrfmodule_vbus_debounce db;
	int64_t delay = DELAY_SENTINEL;

	nrfmodule_vbus_debounce_init(&db, false);

	(void)nrfmodule_vbus_debounce_feed(&db, true, 0);
	zassert_true(nrfmodule_vbus_debounce_next_timeout(&db, STABLE_MS + 5,
							  &delay),
		     "still pending when overdue");
	zassert_equal(delay, 0, "overdue clamps to zero");
}

ZTEST(vbus_debounce, test_full_plug_unplug_cycle)
{
	struct nrfmodule_vbus_debounce db;
	int64_t t = 0;

	nrfmodule_vbus_debounce_init(&db, false);

	(void)nrfmodule_vbus_debounce_feed(&db, true, t);
	t += STABLE_MS;
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, true, t), EV_RISE,
		      "plug settles");

	t += 5000;
	(void)nrfmodule_vbus_debounce_feed(&db, false, t);
	t += STABLE_MS;
	zassert_equal(nrfmodule_vbus_debounce_feed(&db, false, t), EV_FALL,
		      "unplug settles");
	zassert_false(nrfmodule_vbus_debounce_level(&db), "back to absent");
}
