/*
 * Copyright (c) 2024 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_MODEM_LIB_H_
#define NRF_MODEM_LIB_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Library initialization mode.
 * (Kept for internal use, though not exposed in init signature anymore)
 */
enum nrf_modem_mode {
	NRF_MODEM_MODE_NORMAL,
	NRF_MODEM_MODE_LOW_POWER,
};

/**
 * @brief Modem library dfu callback struct.
 */
struct nrf_modem_lib_dfu_cb {
	void (*callback)(int dfu_res, void *ctx);
	void *context;
};

/**
 * @brief Modem library initialization callback struct.
 */
struct nrf_modem_lib_init_cb {
	void (*callback)(int ret, void *ctx);
	void *context;
};

/**
 * @brief Modem library shutdown callback struct.
 */
struct nrf_modem_lib_shutdown_cb {
	void (*callback)(void *ctx);
	void *context;
};

/**
 * @brief AT CFUN callback entry.
 */
struct nrf_modem_lib_at_cfun_cb {
	void (*callback)(int mode, void *ctx);
	void *context;
};

/**
 * @brief Define a callback for DFU result.
 */
#define NRF_MODEM_LIB_ON_DFU_RES(name, _callback, _context)                                        \
	static void _callback(int dfu_res, void *ctx);                                             \
	STRUCT_SECTION_ITERABLE(nrf_modem_lib_dfu_cb, nrf_modem_dfu_hook_##name) = {               \
		.callback = _callback,                                                             \
		.context = _context,                                                               \
	}

/**
 * @brief Define a callback for modem library initialization.
 */
#define NRF_MODEM_LIB_ON_INIT(name, _callback, _context)                                           \
	static void _callback(int ret, void *ctx);                                                 \
	STRUCT_SECTION_ITERABLE(nrf_modem_lib_init_cb, nrf_modem_hook_##name) = {                  \
		.callback = _callback,                                                             \
		.context = _context,                                                               \
	}

/**
 * @brief Define a callback for modem library shutdown.
 */
#define NRF_MODEM_LIB_ON_SHUTDOWN(name, _callback, _context)                                       \
	static void _callback(void *ctx);                                                          \
	STRUCT_SECTION_ITERABLE(nrf_modem_lib_shutdown_cb, nrf_modem_hook_##name) = {              \
		.callback = _callback,                                                             \
		.context = _context,                                                               \
	}

/**
 * @brief Define a callback for successful AT CFUN calls.
 */
#define NRF_MODEM_LIB_ON_CFUN(name, _callback, _context)                                           \
	static void _callback(int mode, void *ctx);                                                \
	STRUCT_SECTION_ITERABLE(nrf_modem_lib_at_cfun_cb, nrf_modem_at_cfun_hook_##name) = {       \
		.callback = _callback,                                                             \
		.context = _context,                                                               \
	}

/**
 * @brief Initialize the Modem library in normal mode.
 */
int nrf_modem_lib_init(void);

/**
 * @brief Initialize the Modem library in bootloader mode.
 */
int nrf_modem_lib_bootloader_init(void);

/**
 * @brief Shutdown the Modem library.
 */
int nrf_modem_lib_shutdown(void);

struct nrf_modem_fault_info {
	uint32_t reason;
	uint32_t program_counter;
	uint32_t signature;
};

void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info);

#ifdef __cplusplus
}
#endif

#endif /* NRF_MODEM_LIB_H_ */
