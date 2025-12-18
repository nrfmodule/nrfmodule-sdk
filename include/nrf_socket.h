/*
 * Copyright (c) 2024 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_SOCKET_H__
#define NRF_SOCKET_H__

#include <zephyr/net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Map Nordic types to Zephyr types */
#define nrf_in_addr  in_addr
#define nrf_in6_addr in6_addr
#define nrf_sa_family_t sa_family_t

#define NRF_AF_INET AF_INET
#define NRF_AF_INET6 AF_INET6

/**
 * @brief Security tag type.
 * Defined here because it is often used in socket contexts.
 */
typedef int nrf_sec_tag_t;

/**
 * @brief Security tag base for TLS decryption.
 * Used by modem_key_mgmt to filter internal tags.
 */
#define NRF_SEC_TAG_TLS_DECRYPT_BASE 2147483648U


/* Map functions */
#define nrf_inet_pton zsock_inet_pton

/**
 * @brief Set DNS server address.
 *
 * @param family Address family (NRF_AF_INET or NRF_AF_INET6).
 * @param addr Pointer to the address structure.
 * @param size Size of the address structure.
 *
 * @return 0 on success, -1 on error.
 */
int nrf_setdnsaddr(int family, const void *addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* NRF_SOCKET_H__ */
