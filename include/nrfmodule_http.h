/*
 * Copyright (c) 2025 nRFModule
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRFMODULE_HTTP_H_
#define NRFMODULE_HTTP_H_

/**
 * @file nrfmodule_http.h
 * @brief HTTP/HTTPS client using Serial Modem AT socket commands.
 *
 * This library provides HTTP and HTTPS client functionality using the
 * AT socket commands available in the Nordic Serial Modem firmware:
 * - AT#XSOCKET / AT#XSSOCKET (open socket)
 * - AT#XCONNECT (connect to server)
 * - AT#XSEND (send data)
 * - AT#XRECV (receive data)
 * - AT#XCLOSE (close socket)
 *
 * Unlike the deprecated AT#XHTTPCCON commands, this approach works with
 * all versions of the Serial Modem firmware and provides more flexibility.
 *
 * Features:
 * - HTTP and HTTPS support
 * - GET, POST, PUT, DELETE, PATCH methods
 * - Custom headers
 * - Body payload for POST/PUT/PATCH
 * - TLS certificate verification via security tags
 * - Streaming response callback for large responses
 * - Compatible with power management (modem can sleep between requests)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/net/tls_credentials.h>

/**
 * @brief HTTP request methods.
 */
enum nrfmodule_http_method {
	NRFMODULE_HTTP_GET,
	NRFMODULE_HTTP_POST,
	NRFMODULE_HTTP_PUT,
	NRFMODULE_HTTP_DELETE,
	NRFMODULE_HTTP_PATCH,
};

/**
 * @brief HTTP request configuration.
 */
struct nrfmodule_http_request {
	/** HTTP method (GET, POST, etc.) */
	enum nrfmodule_http_method method;

	/** Target hostname (e.g., "api.example.com") */
	const char *host;

	/** Target port (0 = auto: 80 for HTTP, 443 for HTTPS) */
	uint16_t port;

	/** Request path (e.g., "/api/v1/data"). NULL defaults to "/" */
	const char *path;

	/** Use HTTPS (TLS). Requires sec_tag with valid certificate. */
	bool secure;

	/** Security tag for TLS certificate (used when secure=true) */
	sec_tag_t sec_tag;

	/** Request body data (for POST/PUT/PATCH). Can be NULL. */
	const char *body;

	/** Request body length in bytes */
	size_t body_len;

	/** Content-Type header value (e.g., "application/json"). Optional. */
	const char *content_type;

	/** Additional custom headers (must end with \r\n for each). Optional. */
	const char *headers;
};

/**
 * @brief HTTP response data passed to callback.
 */
struct nrfmodule_http_response {
	/** HTTP status code (e.g., 200, 404, 500) */
	int status_code;

	/** Content-Length from response headers (may be 0 for chunked) */
	size_t content_length;

	/** Pointer to current body fragment (may be called multiple times) */
	const char *body;

	/** Length of current body fragment */
	size_t body_len;

	/** True if this is the final callback (no more data) */
	bool is_final;

	/** Error code if request failed (0 on success) */
	int error;
};

/**
 * @brief Response callback function type.
 *
 * This callback may be called multiple times:
 * 1. Once for each received body fragment (is_final=false)
 * 2. Once at the end with is_final=true
 *
 * If an error occurs, the callback is called once with error set and is_final=true.
 *
 * @param response Response data (status code, body fragment, etc.)
 * @param user_data User data passed to nrfmodule_http_request()
 */
typedef void (*nrfmodule_http_response_cb_t)(struct nrfmodule_http_response *response,
					     void *user_data);

/**
 * @brief Initialize the HTTP client.
 *
 * Must be called once before making any HTTP requests.
 *
 * @return 0 on success, negative errno on failure
 */
int nrfmodule_http_init(void);

/**
 * @brief Perform an HTTP request.
 *
 * This function is blocking - it will not return until the request completes,
 * times out, or is cancelled.
 *
 * The response callback is called one or more times as data is received:
 * - For each body fragment received
 * - Finally with is_final=true when the response is complete
 *
 * @param req Request configuration
 * @param rsp Response structure to populate (can be NULL if using callback only)
 * @param cb Response callback for streaming response (can be NULL for simple requests)
 * @param user_data User data passed to callback
 * @param timeout_ms Request timeout in milliseconds (0 = use default)
 *
 * @return 0 on success, negative errno on failure:
 *         -EINVAL: Invalid parameters
 *         -EBUSY: Another request in progress
 *         -ETIMEDOUT: Request timed out
 *         -ECONNREFUSED: Connection refused
 *         -ENOTCONN: Not connected
 *         -ENOMEM: Out of memory
 */
int nrfmodule_http_request(const struct nrfmodule_http_request *req,
			   struct nrfmodule_http_response *rsp,
			   nrfmodule_http_response_cb_t cb,
			   void *user_data,
			   int32_t timeout_ms);

/**
 * @brief Cancel an ongoing HTTP request.
 *
 * If a request is in progress, it will be aborted and the callback
 * will be called with is_final=true and error=-ECANCELED.
 *
 * @return 0 on success (or if no request was in progress)
 */
int nrfmodule_http_cancel(void);

/* --- Convenience Macros --- */

/**
 * @brief Simple HTTP GET request.
 *
 * Example:
 * @code
 * struct nrfmodule_http_response rsp;
 * int err = nrfmodule_http_get("api.example.com", "/data", false, 0, &rsp, 30000);
 * @endcode
 */
#define nrfmodule_http_get(_host, _path, _secure, _sec_tag, _rsp, _timeout_ms) \
	nrfmodule_http_request(&(struct nrfmodule_http_request){ \
		.method = NRFMODULE_HTTP_GET, \
		.host = (_host), \
		.path = (_path), \
		.secure = (_secure), \
		.sec_tag = (_sec_tag), \
	}, (_rsp), NULL, NULL, (_timeout_ms))

/**
 * @brief Simple HTTPS GET request with certificate.
 */
#define nrfmodule_https_get(_host, _path, _sec_tag, _rsp, _timeout_ms) \
	nrfmodule_http_get((_host), (_path), true, (_sec_tag), (_rsp), (_timeout_ms))

/**
 * @brief Simple HTTP POST request with JSON body.
 */
#define nrfmodule_http_post_json(_host, _path, _secure, _sec_tag, _json, _rsp, _timeout_ms) \
	nrfmodule_http_request(&(struct nrfmodule_http_request){ \
		.method = NRFMODULE_HTTP_POST, \
		.host = (_host), \
		.path = (_path), \
		.secure = (_secure), \
		.sec_tag = (_sec_tag), \
		.body = (_json), \
		.body_len = strlen(_json), \
		.content_type = "application/json", \
	}, (_rsp), NULL, NULL, (_timeout_ms))

#ifdef __cplusplus
}
#endif

#endif /* NRFMODULE_HTTP_H_ */
