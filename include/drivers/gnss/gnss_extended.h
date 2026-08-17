/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gnss_extended.h
 * @brief Extended GNSS data beyond Zephyr's standard gnss_data.
 *
 * Provides position-accuracy fields alongside the standard fix. Call
 * gnss_extended_get_accuracy() from your GNSS_DATA_CALLBACK_DEFINE
 * handler to get position error alongside the standard fix.
 *
 * This API is generic and not tied to any specific GNSS module. The
 * accuracy source is backend-specific (e.g. NMEA GST sentences, or the
 * fix report of a modem-hosted receiver).
 */

#ifndef NRFMODULE_DRIVERS_GNSS_EXTENDED_H_
#define NRFMODULE_DRIVERS_GNSS_EXTENDED_H_

#include <zephyr/device.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GNSS accuracy data for the latest fix (source is backend-specific).
 */
struct gnss_accuracy {
	/** Horizontal accuracy in millimeters (sqrt(lat_sigma^2 + lon_sigma^2)). */
	uint32_t h_accuracy_mm;

	/** Vertical accuracy in millimeters (from GST alt sigma, 0 if unavailable). */
	uint32_t v_accuracy_mm;

	/**
	 * UTC timestamp of the GST sentence (milliseconds from midnight,
	 * same encoding as gnss_parse_dec_to_milli on the UTC field).
	 * Compare with the fix's UTC to check if accuracy is from the
	 * current epoch or the previous one.
	 */
	uint32_t utc;

	/**
	 * true if at least one GST sentence has been received since power-on.
	 * false on the first fix (no GST data yet).
	 */
	bool valid;
};

/**
 * @brief Get the latest GNSS accuracy data.
 *
 * Call this from your GNSS_DATA_CALLBACK_DEFINE handler to get position
 * accuracy alongside the standard fix. Depending on the backend the
 * accuracy may lag the fix by one epoch - compare utc fields to detect
 * this (NMEA backends: GST arrives after GGA/RMC).
 *
 * Thread-safe: copies data under a dedicated spinlock, not the driver's
 * command lock — non-blocking, safe to call from the GNSS data callback,
 * and cannot deadlock against PM actions.
 *
 * @param dev      GNSS device.
 * @param accuracy Output struct filled on success.
 *
 * @retval 0        Success.
 * @retval -EINVAL  NULL parameter.
 * @retval -ENOTSUP Device driver does not support accuracy data.
 */
int gnss_extended_get_accuracy(const struct device *dev,
			       struct gnss_accuracy *accuracy);

/**
 * @brief GNSS power mode.
 */
enum gnss_power_mode {
	/** Full power - continuous tracking, highest current. */
	GNSS_POWER_MODE_FULL = 0,

	/** Low power - the receiver autonomously manages sleep/wake.
	 *  Best for continuous tracking with power savings. (Value matches
	 *  the PMTK AlwaysLocate mode of the first backend; kept for ABI.) */
	GNSS_POWER_MODE_LOW = 9,

	/** Periodic - run for a fixed time, sleep for a fixed time.
	 *  Requires run_ms and sleep_ms parameters. */
	GNSS_POWER_MODE_PERIODIC = 1,
};

/**
 * @brief Set the GNSS module power mode.
 *
 * Controls the receiver's internal sleep/wake cycling. For PERIODIC mode,
 * provide run and sleep durations. For FULL and LOW modes, run_ms and
 * sleep_ms are ignored.
 *
 * May block while the backend talks to the receiver (e.g. an AT round-trip
 * on a modem-hosted receiver, up to the AT command timeout); call from
 * thread context only. Backends with a FORCE_ON pin release it for
 * low-power modes and re-assert it when returning to FULL.
 *
 * @param dev      GNSS device.
 * @param mode     Power mode to set.
 * @param run_ms   Run time in ms (PERIODIC only, 0 for other modes).
 * @param sleep_ms Sleep time in ms (PERIODIC only, 0 for other modes).
 *
 * @retval 0        Success.
 * @retval -EINVAL  Invalid parameters (e.g., PERIODIC with 0 run/sleep).
 * @retval -ENOTSUP Device does not support power mode control.
 * @retval -EIO     Command failed.
 */
int gnss_extended_set_power_mode(const struct device *dev,
				 enum gnss_power_mode mode,
				 uint32_t run_ms, uint32_t sleep_ms);

/**
 * @brief Get the current GNSS power mode.
 *
 * @param dev  GNSS device.
 * @param mode Output for current power mode.
 *
 * @retval 0        Success.
 * @retval -EINVAL  NULL parameter.
 * @retval -ENOTSUP Device does not support power mode query.
 */
int gnss_extended_get_power_mode(const struct device *dev,
				 enum gnss_power_mode *mode);

#ifdef __cplusplus
}
#endif

#endif /* NRFMODULE_DRIVERS_GNSS_EXTENDED_H_ */
