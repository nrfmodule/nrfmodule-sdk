/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gnss_assist.h
 * @brief GNSS assistance data upload API.
 *
 * Generic API for uploading assistance data (EPO, AGPS, almanac) to a GNSS
 * module. The upload engine reads data in chunks via a caller-provided
 * callback, so the full file never needs to fit in RAM.
 *
 * Chip-specific framing (MTK binary, UBX, etc.) is handled by transport
 * implementations selected at build time via Kconfig.
 */

#ifndef NRFMODULE_DRIVERS_GNSS_ASSIST_H_
#define NRFMODULE_DRIVERS_GNSS_ASSIST_H_

#include <zephyr/device.h>
#include <zephyr/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read callback — called by the upload engine to fetch the next chunk.
 *
 * The engine calls this to fill its work buffer with assistance data.
 * The callback reads from whatever source the app uses (flash, BLE stream,
 * HTTP download buffer, etc.).
 *
 * @param buf      Engine-owned buffer to fill.
 * @param buf_size Number of bytes to read (one packet payload).
 * @param offset   Byte offset into the assistance data file.
 * @param user_data Opaque context passed through from gnss_assist_params.
 *
 * @retval positive Number of bytes actually read.
 * @retval 0        End of data.
 * @retval negative errno on read error.
 */
typedef int (*gnss_assist_read_cb_t)(uint8_t *buf, size_t buf_size,
				     size_t offset, void *user_data);

/**
 * @brief Progress callback — called after each packet is acknowledged.
 *
 * Use this to feed watchdogs, update status LEDs, log progress, or cancel
 * the upload. Returning a negative value aborts the upload cleanly (the
 * engine exits transfer mode before returning the error).
 *
 * @param dev           GNSS device.
 * @param packets_sent  Number of packets successfully sent so far.
 * @param packets_total Total number of packets to send.
 * @param user_data     Opaque context passed through from gnss_assist_params.
 *
 * @retval 0        Continue uploading.
 * @retval negative Abort upload (value is propagated as the return code).
 */
typedef int (*gnss_assist_progress_cb_t)(const struct device *dev,
					 uint32_t packets_sent,
					 uint32_t packets_total,
					 void *user_data);

/**
 * @brief Parameters for gnss_assist_upload().
 */
struct gnss_assist_params {
	/** Callback to read assistance data chunks. Required. */
	gnss_assist_read_cb_t reader;

	/** Progress callback. NULL to disable. */
	gnss_assist_progress_cb_t progress;

	/** Opaque context passed to reader and progress callbacks. */
	void *user_data;

	/** Total size of the assistance data in bytes. */
	size_t total_size;

	/**
	 * Work buffer provided by the caller. Must be at least
	 * gnss_assist_get_buf_size() bytes. The engine uses this buffer
	 * for packet framing — one packet at a time.
	 */
	uint8_t *buf;

	/** Size of the work buffer in bytes. */
	size_t buf_size;
};

/**
 * @brief Get the required work buffer size for assistance upload.
 *
 * Returns the minimum buffer size needed for gnss_assist_params.buf.
 * This depends on the chip's packet size (e.g., 227 bytes for MTK).
 *
 * @param dev GNSS device.
 *
 * @retval positive Required buffer size in bytes.
 * @retval -ENOTSUP Device does not support assistance upload.
 */
int gnss_assist_get_buf_size(const struct device *dev);

/**
 * @brief Upload assistance data to a GNSS module.
 *
 * Blocks the calling thread for the duration of the upload. The engine:
 * 1. Enters chip-specific transfer mode (e.g., NMEA -> MTK binary).
 * 2. Reads data chunks via params->reader, frames and sends each packet.
 * 3. Waits for per-packet acknowledgement from the module.
 * 4. Calls params->progress (if set) after each ACK.
 * 5. Exits transfer mode and returns.
 *
 * On any error (read failure, NACK, timeout, progress abort), the engine
 * cleans up (exits transfer mode) before returning the error code.
 *
 * @param dev    GNSS device.
 * @param params Upload parameters. Must remain valid for the call duration.
 *
 * @retval 0        Upload complete.
 * @retval -EINVAL  Invalid parameters (NULL reader, buffer too small, etc.).
 * @retval -ENOMEM  Work buffer too small (use gnss_assist_get_buf_size()).
 * @retval -ENOTSUP Device does not support assistance upload.
 * @retval -EIO     Transport error (send or ACK failure).
 * @retval -ETIMEDOUT ACK timeout from the module.
 * @retval negative Other error from reader or progress callback.
 */
int gnss_assist_upload(const struct device *dev,
		       const struct gnss_assist_params *params);

/**
 * @brief EPO status as reported by the GNSS module itself (PMTK707).
 *
 * Populated by gnss_assist_query_epo(). All fields are zero when valid
 * is false.
 */
struct gnss_assist_epo_status {
	/** UNIX timestamp of the start of EPO coverage. */
	uint32_t start_time;

	/** UNIX timestamp of the end of EPO coverage. */
	uint32_t end_time;

	/** Number of EPO data sets stored in the module. */
	uint16_t epo_sets;

	/** true if the module has EPO data loaded (epo_sets > 0). */
	bool valid;
};

/**
 * @brief Query the EPO status from the GNSS module (PMTK607/707).
 *
 * Sends $PMTK607 and parses the $PMTK707 response to determine whether
 * the module has EPO data loaded and what time range it covers.
 *
 * The calling thread blocks until the response is received or a timeout
 * occurs. The GPS must be powered on before calling this function.
 *
 * @param dev    GNSS device.
 * @param status Output struct filled on success.
 *
 * @retval 0          Query succeeded; status is populated.
 * @retval -EINVAL    NULL parameter.
 * @retval -ENOTSUP   Device does not support EPO status query.
 * @retval -ETIMEDOUT No response from module within timeout.
 * @retval -EIO       Transport error.
 */
int gnss_assist_query_epo(const struct device *dev,
			  struct gnss_assist_epo_status *status);

#ifdef __cplusplus
}
#endif

#endif /* NRFMODULE_DRIVERS_GNSS_ASSIST_H_ */
