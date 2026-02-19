/******************************************************************************
 * Copyright (c) 2025 Marconatale Parise.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/
/**
 * @file http_hal.h
 * @brief HTTP server abstraction layer (HAL) for esp_http_server
 * This module provides a simple wrapper around esp_http_server to manage server lifecycle
 * and endpoint registration. It allows registering endpoints before starting the server and handles common response patterns.
 * @author Marconatale Parise   
 * @date 19 Feb 2026
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for the HTTP HAL instance
 *
 * This handle encapsulates an esp_http_server instance and a simple
 * endpoint registration layer.
 */
typedef struct http_hal_s http_hal_t;

/**
 * @brief Endpoint handler callback type
 *
 * This is the same signature used by esp_http_server handlers.
 *
 * @param req Incoming HTTP request
 * @return ESP_OK on success, otherwise an error code
 */
typedef esp_err_t (*http_hal_handler_t)(httpd_req_t *req);

/**
 * @brief HTTP HAL configuration
 *
 * User-modifiable knobs:
 * - port: Listening port for the HTTP server (0 uses HTTPD_DEFAULT_CONFIG)
 * - lru_purge_enable: Enable LRU purge to free least recently used sessions
 * - max_uri_handlers: Max number of URI handlers (0 uses default)
 */
typedef struct {
    int  port;
    bool lru_purge_enable;
    int  max_uri_handlers;
} http_hal_config_t;

/**
 * @brief HTTP endpoint descriptor
 *
 * This describes an URI handler that will be registered into the underlying
 * esp_http_server instance.
 *
 * Notes:
 * - uri must remain valid for the lifetime of the server (static string
 *   literal recommended).
 */
typedef struct {
    const char           *uri;
    httpd_method_t        method;
    http_hal_handler_t    handler;
    void                *user_ctx;
} http_hal_endpoint_t;

/**
 * @brief Initialize an HTTP HAL instance (does not start the server)
 *
 * This function allocates and initializes the internal HAL object.
 * The HTTP server is not started until http_hal_start() is called.
 *
 * @param[out] out Returned HAL instance pointer
 * @param[in]  cfg Configuration parameters
 * @return ESP_OK on success
 */
esp_err_t http_hal_init(http_hal_t **out, const http_hal_config_t *cfg);

/**
 * @brief Start the HTTP server
 *
 * If endpoints were registered before calling this function,
 * they will be registered into the server during startup.
 *
 * Calling this function multiple times is safe; if already started,
 * it returns ESP_OK.
 *
 * @param[in] h HAL instance
 * @return ESP_OK on success
 */
esp_err_t http_hal_start(http_hal_t *h);

/**
 * @brief Stop the HTTP server
 *
 * Calling this function multiple times is safe; if already stopped,
 * it returns ESP_OK.
 *
 * @param[in] h HAL instance
 * @return ESP_OK on success
 */
esp_err_t http_hal_stop(http_hal_t *h);

/**
 * @brief Deinitialize the HTTP HAL instance
 *
 * This function stops the server if running, then frees internal resources.
 *
 * @param[in] h HAL instance (can be NULL)
 */
void http_hal_deinit(http_hal_t *h);

/**
 * @brief Register an HTTP endpoint (URI handler)
 *
 * This function can be called before or after http_hal_start().
 * - If called before start: the endpoint is stored and registered on start.
 * - If called after start: the endpoint is registered immediately.
 *
 * Notes:
 * - ep->uri must remain valid for the lifetime of the server (static string
 *   literal recommended).
 *
 * @param[in] h  HAL instance
 * @param[in] ep Endpoint descriptor
 * @return ESP_OK on success
 */
esp_err_t http_hal_register_endpoint(http_hal_t *h, const http_hal_endpoint_t *ep);

/**
 * @brief Unregister an HTTP endpoint
 *
 * This function requires the server to be running.
 *
 * @param[in] h      HAL instance
 * @param[in] uri    Endpoint URI to unregister
 * @param[in] method HTTP method of the endpoint
 * @return ESP_OK on success
 */
esp_err_t http_hal_unregister_endpoint(http_hal_t *h, const char *uri, httpd_method_t method);

/**
 * @brief Get native esp_http_server handle
 *
 * This is useful when you need to call esp_http_server APIs directly.
 *
 * @param[in] h HAL instance
 * @return Native httpd_handle_t or NULL if not started/invalid
 */
httpd_handle_t http_hal_native_handle(http_hal_t *h);

/**
 * @brief Send a JSON response with a specific HTTP status code
 *
 * This helper sets:
 * - Status: status_code (numeric)
 * - Content-Type: application/json
 *
 * @param[in] req         Incoming HTTP request
 * @param[in] status_code HTTP status code (e.g., 200, 400, 500)
 * @param[in] json        Null-terminated JSON payload
 * @return ESP_OK on success
 */
esp_err_t http_hal_send_json(httpd_req_t *req, int status_code, const char *json);

/**
 * @brief Send an error response with a specific HTTP status code
 *
 * This helper tries to use httpd_resp_send_err() for common codes and
 * falls back to a JSON error payload otherwise.
 *
 * @param[in] req         Incoming HTTP request
 * @param[in] status_code HTTP status code (e.g., 400, 401, 404, 500)
 * @param[in] msg         Error message
 * @return ESP_OK on success
 */
esp_err_t http_hal_send_err(httpd_req_t *req, int status_code, const char *msg);

#ifdef __cplusplus
}
#endif
