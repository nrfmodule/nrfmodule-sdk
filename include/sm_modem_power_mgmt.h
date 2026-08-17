/*
 * Copyright (c) 2025 nRFModule
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SM_MODEM_POWER_MGMT_H
#define SM_MODEM_POWER_MGMT_H

#include <zephyr/kernel.h>

/**
 * @file sm_modem_power_mgmt.h
 * @brief Serial Modem Power Management API
 * 
 * This module provides automatic power management for serial modems that support
 * AT#XSLEEP command (e.g., nRF9160, nRF9161). It:
 * - Tracks modem sleep state (AWAKE/IDLE)
 * - Automatically wakes modem when sending AT commands
 * - Puts modem to sleep after inactivity timeout
 * - Works with DTR UART control in sm_at_client
 */

/** Modem power states returned by sm_modem_power_mgmt_get_state() */
enum sm_modem_power_state {
	SM_MODEM_STATE_UNKNOWN = 0,
	SM_MODEM_STATE_AWAKE   = 1,
	SM_MODEM_STATE_IDLE    = 2,
};

/**
 * @brief Initialize modem power management
 * 
 * This must be called after sm_at_client_init() and before any AT traffic
 * goes out through the nrf_modem_at API.
 * 
 * @param inactivity_timeout Time before automatically sending AT#XSLEEP=2
 *                           Use K_NO_WAIT to disable auto-sleep
 * 
 * @return 0 on success, negative errno on failure
 */
int sm_modem_power_mgmt_init(k_timeout_t inactivity_timeout);

/**
 * @brief Ensure the modem is awake without sending a command.
 *
 * Wakes the modem if it is in IDLE state and waits until it is responsive.
 * Does NOT send any user command. Useful when the caller needs to guarantee
 * the modem is awake before managing its own TX/RX buffer.
 *
 * @return 0 on success, negative errno on failure
 */
int sm_modem_power_mgmt_ensure_awake(void);

/**
 * @brief Notify the power manager that AT activity just occurred.
 *
 * Resets the inactivity timer so the modem is not put to sleep prematurely.
 * Call this after every successful AT command sent outside the normal
 * nrf_modem_at_*() path.
 */
void sm_modem_power_mgmt_notify_activity(void);

/**
 * @brief Pause automatic sleep (e.g. during LTE registration).
 *
 * Cancels any pending inactivity timer. Auto-sleep remains paused
 * until sm_modem_power_mgmt_resume() is called.
 */
void sm_modem_power_mgmt_pause(void);

/**
 * @brief Resume automatic sleep after a previous pause.
 *
 * Restarts the inactivity timer from now.
 */
void sm_modem_power_mgmt_resume(void);

/**
 * @brief Manually put modem to sleep immediately
 * 
 * Stops the inactivity timer and sends AT#XSLEEP=2 immediately.
 * Useful when application knows it won't need the modem for a while.
 * 
 * @return 0 on success, negative errno on failure
 */
int sm_modem_power_mgmt_sleep(void);

/**
 * @brief Get current modem power state (for debugging)
 *
 * @return enum modem_power_state value
 */
enum sm_modem_power_state sm_modem_power_mgmt_get_state(void);

#endif /* SM_MODEM_POWER_MGMT_H */
