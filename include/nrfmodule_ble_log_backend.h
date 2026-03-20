/*
 * Copyright (c) 2026 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_BLE_LOG_BACKEND_H_
#define NRFMODULE_BLE_LOG_BACKEND_H_

/**
 * @file
 * @brief nRFModule BLE log backend API
 *
 * BLE log backend with custom UUID that coexists with Shell BT NUS.
 * The service UUID differs from standard NUS (last byte 0x00 vs 0x9E)
 * so both can run simultaneously:
 *   - Shell: standard NUS UUID (0x6E400001...9E)
 *   - Logs:  custom UUID       (0x6E400001...00)
 */

#include <stdbool.h>

/**
 * @brief Raw advertising UUID data for the nRFModule BLE log service.
 *
 * Use in BT_DATA_BYTES(BT_DATA_UUID128_ALL, ...) to advertise the service.
 */
#define NRFMODULE_BLE_LOG_ADV_UUID_DATA \
	0x00, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
	0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E

/**
 * @brief Hook called when the log backend is enabled or disabled.
 *
 * @param enabled  true when a client subscribes to notifications, false when it unsubscribes
 * @param ctx      User context passed to nrfmodule_ble_log_set_hook()
 */
typedef void (*nrfmodule_ble_log_hook_t)(bool enabled, void *ctx);

/**
 * @brief Register a hook for log backend status changes.
 *
 * @param hook  Callback function
 * @param ctx   User context
 */
void nrfmodule_ble_log_set_hook(nrfmodule_ble_log_hook_t hook, void *ctx);

#endif /* NRFMODULE_BLE_LOG_BACKEND_H_ */
