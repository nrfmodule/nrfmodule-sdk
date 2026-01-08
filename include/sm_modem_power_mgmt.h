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

/**
 * @brief Initialize modem power management
 * 
 * This must be called after sm_at_client_init() and before sending
 * any AT commands through sm_modem_power_mgmt_send_at().
 * 
 * @param inactivity_timeout Time before automatically sending AT#XSLEEP=2
 *                           Use K_NO_WAIT to disable auto-sleep
 * 
 * @return 0 on success, negative errno on failure
 */
int sm_modem_power_mgmt_init(k_timeout_t inactivity_timeout);

/**
 * @brief Send AT command with automatic power management
 * 
 * This function:
 * 1. Wakes the modem if in IDLE state (handles AT#XSLEEP wake timing)
 * 2. Sends the AT command via sm_at_client
 * 3. Resets the inactivity timer
 * 
 * Use this instead of calling sm_at_client_send_cmd() directly when
 * power management is enabled.
 * 
 * @param cmd AT command string (without terminator)
 * @param timeout Command timeout in seconds (0 = wait forever)
 * 
 * @return AT command result (AT_CMD_OK, AT_CMD_ERROR, etc.) or negative errno
 */
int sm_modem_power_mgmt_send_at(const char *cmd, uint32_t timeout);

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
 * @return 0=UNKNOWN, 1=AWAKE, 2=IDLE
 */
int sm_modem_power_mgmt_get_state(void);

#endif /* SM_MODEM_POWER_MGMT_H */
