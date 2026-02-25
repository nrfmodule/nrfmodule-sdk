/*
 * Copyright (c) 2024 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_MODEM_H__
#define NRF_MODEM_H__

#include <nrf_modem_lib.h>
#include <nrf_modem_at.h>
/* Add other headers if needed, e.g. nrf_modem_gnss.h */

/**
 * @brief Security tag type.
 */
typedef int nrf_sec_tag_t;

struct nrf_modem_fault_info {
        uint32_t reason;
        uint32_t program_counter;
};

#endif /* NRF_MODEM_H__ */
