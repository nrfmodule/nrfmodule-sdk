/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pure USB VBUS debounce state machine. No kernel calls, time is injected.
 */

#include <nrfmodule_vbus_debounce.h>

/* Settle a pending candidate whose window has expired. */
static enum nrfmodule_vbus_debounce_event
settle(struct nrfmodule_vbus_debounce *db, int64_t now_ms)
{
	if (!db->pending || now_ms < db->deadline_ms) {
		return NRFMODULE_VBUS_DEBOUNCE_EVENT_NONE;
	}

	db->level = db->candidate;
	db->pending = false;

	return db->level ? NRFMODULE_VBUS_DEBOUNCE_EVENT_RISE
			 : NRFMODULE_VBUS_DEBOUNCE_EVENT_FALL;
}

void nrfmodule_vbus_debounce_init(struct nrfmodule_vbus_debounce *db,
				  bool initial_level)
{
	db->level = initial_level;
	db->candidate = initial_level;
	db->pending = false;
	db->deadline_ms = 0;
}

enum nrfmodule_vbus_debounce_event
nrfmodule_vbus_debounce_feed(struct nrfmodule_vbus_debounce *db, bool raw_level,
			     int64_t now_ms)
{
	if (raw_level != db->candidate) {
		db->candidate = raw_level;

		if (raw_level == db->level) {
			/* Bounced back home: drop the pending change. */
			db->pending = false;
		} else {
			db->pending = true;
			db->deadline_ms =
				now_ms + NRFMODULE_VBUS_DEBOUNCE_STABLE_MS;
		}
	}

	return settle(db, now_ms);
}

enum nrfmodule_vbus_debounce_event
nrfmodule_vbus_debounce_tick(struct nrfmodule_vbus_debounce *db, int64_t now_ms)
{
	return settle(db, now_ms);
}

bool nrfmodule_vbus_debounce_level(const struct nrfmodule_vbus_debounce *db)
{
	return db->level;
}

bool nrfmodule_vbus_debounce_next_timeout(
	const struct nrfmodule_vbus_debounce *db, int64_t now_ms,
	int64_t *delay_ms)
{
	if (!db->pending) {
		return false;
	}

	*delay_ms = (db->deadline_ms > now_ms) ? (db->deadline_ms - now_ms) : 0;

	return true;
}
