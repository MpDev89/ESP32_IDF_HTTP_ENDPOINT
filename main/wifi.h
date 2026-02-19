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
 * @file wifi.h
 * @brief Wi-Fi connection management (esp-netif and event loop)
 * This module provides functions to initialize Wi-Fi, connect in STA mode, and
 * disable power-save mode for better OTA performance. It also exposes the esp-netif handle for
 * the STA interface.
 * @author Marconatale Parise
 * @date 19 Feb 2026
 */

#pragma once

#include "esp_err.h"
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "sdkconfig.h"

/* Forward declaration to avoid forcing esp_netif.h include in every user */
typedef struct esp_netif_obj esp_netif_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi connection (esp-netif and event loop)
 *
 * Must be called before wifi_connect_sta().
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_init_connection(void);

/**
 * @brief Connect to a Wi-Fi AP in STA mode (blocking)
 *
 * This module replaces protocol_examples_common's example_connect().
 *
 * User-modifiable knobs are exposed via Kconfig:
 * - CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN
 * - CONFIG_EXAMPLE_WIFI_MAX_RETRY
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_connect_sta(void);

/**
 * @brief Disable Wi-Fi power-save mode (recommended for OTA throughput)
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_disable_powersave(void);

/**
 * @brief Get the STA esp_netif handle created by wifi_connect_sta()
 *
 * @return Pointer to esp_netif_t, or NULL if Wi-Fi is not initialized
 */
esp_netif_t *wifi_get_netif_sta(void);

#ifdef __cplusplus
}
#endif
