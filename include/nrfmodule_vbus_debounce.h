/*
 * Copyright (c) 2026 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_VBUS_DEBOUNCE_H_
#define NRFMODULE_VBUS_DEBOUNCE_H_

/**
 * @file
 * @brief Pure USB VBUS debounce state machine.
 *
 * Raw VBUS edges bounce hard on hand plug-in: a single connect can produce
 * dozens of level changes. This filter reports a level change only after the
 * raw level has held steady for the stability window.
 *
 * Pure: no kernel, timer or GPIO calls. Time is injected by the caller, so the
 * state machine is testable off-target. The owner supplies the storage.
 */

#include <stdbool.h>
#include <stdint.h>

/** Raw level must hold this long before the debounced level follows it. */
#define VBUS_DEBOUNCE_STABLE_MS (2000)

/** Result of feeding a sample or running a tick. */
enum vbus_debounce_event {
	/** Debounced level unchanged. */
	VBUS_DEBOUNCE_EVENT_NONE = 0,
	/** Debounced level went low to high (cable attached). */
	VBUS_DEBOUNCE_EVENT_RISE,
	/** Debounced level went high to low (cable removed). */
	VBUS_DEBOUNCE_EVENT_FALL,
};

/** Debounce state. Treat as opaque; use the accessors. */
struct vbus_debounce {
	/** Level reported to consumers. */
	bool level;
	/** Most recent raw sample. */
	bool candidate;
	/** Candidate differs from level and is waiting out the window. */
	bool pending;
	/** Uptime at which a pending candidate becomes the level. */
	int64_t deadline_ms;
};

/**
 * @brief Seed the filter with the level measured at boot.
 *
 * No event is produced for the seed: boot while plugged in has no edge, so
 * consumers must read the level with vbus_debounce_level().
 *
 * @param db             State to initialise
 * @param initial_level  Raw VBUS level at boot
 */
void vbus_debounce_init(struct vbus_debounce *db, bool initial_level);

/**
 * @brief Feed a raw sample and evaluate the window.
 *
 * A sample equal to the previous raw sample does not restart the window. A
 * sample that returns to the debounced level cancels a pending change.
 *
 * @param db         State
 * @param raw_level  Raw VBUS level now
 * @param now_ms     Current uptime in milliseconds
 *
 * @return Event produced by this sample.
 */
enum vbus_debounce_event vbus_debounce_feed(struct vbus_debounce *db,
					    bool raw_level, int64_t now_ms);

/**
 * @brief Evaluate the window without a new sample.
 *
 * Call when the deadline reported by vbus_debounce_next_timeout() expires.
 *
 * @param db      State
 * @param now_ms  Current uptime in milliseconds
 *
 * @return Event produced by the elapsed time.
 */
enum vbus_debounce_event vbus_debounce_tick(struct vbus_debounce *db,
					    int64_t now_ms);

/**
 * @brief Current debounced level.
 *
 * @param db  State
 *
 * @return true when VBUS is considered present.
 */
bool vbus_debounce_level(const struct vbus_debounce *db);

/**
 * @brief Time until the next tick is due.
 *
 * @param db        State
 * @param now_ms    Current uptime in milliseconds
 * @param delay_ms  Out: milliseconds until the deadline, 0 if already due.
 *                  Untouched when no tick is pending.
 *
 * @return true when a tick is pending, false when the filter is settled.
 */
bool vbus_debounce_next_timeout(const struct vbus_debounce *db, int64_t now_ms,
				int64_t *delay_ms);

#endif /* NRFMODULE_VBUS_DEBOUNCE_H_ */
