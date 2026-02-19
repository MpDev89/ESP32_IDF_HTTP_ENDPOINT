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
/** * @file main.c
 * @brief Main application entry point. Initializes Wi-Fi, sets up the HTTP server, and defines the LED control endpoint.
 *
 * This application connects to a Wi-Fi network, starts an HTTP server, and provides a simple API to control an LED connected to a GPIO pin.
 * The API supports both direct level setting (0/1) and logical state (on/off).
 *
 * @author Marconatale Parise
 * @date 19 Feb 2026
 */

#include <string.h>
#include <ctype.h>
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "wifi.h"
#include "common.h"
#include "http_hal.h"

#define GPIO_OUT    CONFIG_GPIO_OUT_PIN
#define GPIO_OUT_PIN_SEL  (1ULL<<GPIO_OUT)
#define LED_ACTIVE_LOW false
#define TASKAPP_TIME 1000 //ms

static esp_err_t gpio_set(uint32_t gpio_num, bool* toogle);
static esp_err_t gpio_init(void);

/* ====== HTTP HAL handle ====== */
static http_hal_t *s_http = NULL;
//uint32_t level = 1;
bool log_request = false;
int lvl, logical = 0;

/* ====== Helpers ====== */
static bool parse_state(const char *s, int *out_level)
{
    if (!s || !out_level) return false;

    char tmp[16];
    size_t n = strnlen(s, sizeof(tmp)-1);
    for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)s[i]);
    tmp[n] = '\0';
    LOG("Parsed state query: '%s'", tmp);

    if (!strcmp(tmp, "1") || !strcmp(tmp, "on") || !strcmp(tmp, "true"))  { *out_level = 1; return true; }
    if (!strcmp(tmp, "0") || !strcmp(tmp, "off")|| !strcmp(tmp, "false")) { *out_level = 0; return true; }
    return false;
}

static int logical_from_gpio_level(int gpio_level)
{
    // gpio_level is the logic level on pin (0/1)
    // logical is "led on?" with active_low
    if (LED_ACTIVE_LOW) return gpio_level ? 0 : 1;
    return gpio_level ? 1 : 0;
}

static int gpio_level_from_logical(int logical_level)
{
    // logical_level = 1 => LED ON
    int lvl = logical_level ? 1 : 0;
    if (LED_ACTIVE_LOW) lvl = !lvl;
    return lvl;
}

/* ====== Handler: GET /api/led ====== */
static esp_err_t led_get_handler(httpd_req_t *req)
{
    char query[96] = {0};
    

    // manage: ?level=0|1 or ?state=on/off/true/false
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {

        // 1) level=0|1 (no logical interpretation, directly set gpio level)
        char level_str[8] = {0};
        if (httpd_query_key_value(query, "level", level_str, sizeof(level_str)) == ESP_OK) {
            if (!parse_state(level_str, &lvl)) {
                return http_hal_send_err(req, 400, "Invalid level (use 0 or 1)");
            }
            gpio_set_level(GPIO_OUT, gpio_level_from_logical(lvl));
        }

        // 2) state=on/off/true/false (logic\al interpretation, set gpio level based on logical state)
        char state_str[16] = {0};
        if (httpd_query_key_value(query, "state", state_str, sizeof(state_str)) == ESP_OK) {
            if (!parse_state(state_str, &logical)) {
                return http_hal_send_err(req, 400, "Invalid state (use on/off/true/false)");
            }
            log_request = true;
            gpio_set_level(GPIO_OUT, gpio_level_from_logical(logical));
        }
    }
    int gpio_lvl;
    if(log_request){
        gpio_lvl = logical;
    }else{
        gpio_lvl = lvl;
    }
    // Reply with current state
    
    int led_on = logical_from_gpio_level(gpio_lvl);

    char resp[80];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"led\":%s,\"gpio_level\":%d}",
             led_on ? "true" : "false",
             gpio_lvl);

    return http_hal_send_json(req, 200, resp);
}

static esp_err_t gpio_init(void)
{
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    return ESP_OK;
}

static void app_setup_http(void)
{
    http_hal_config_t cfg = {
        .port = 0,
        .lru_purge_enable = true,
        .max_uri_handlers = 16
    };
    ESP_ERROR_CHECK(http_hal_init(&s_http, &cfg));

    http_hal_endpoint_t led_ep = {
        .uri = "/api/led",
        .method = HTTP_GET,
        .handler = led_get_handler,
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(http_hal_register_endpoint(s_http, &led_ep));
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(gpio_init());
    app_setup_http();


    ESP_ERROR_CHECK(wifi_init_connection());
    ESP_ERROR_CHECK(wifi_connect_sta());
    ESP_ERROR_CHECK(wifi_disable_powersave());

     // Start server
    ESP_ERROR_CHECK(http_hal_start(s_http));

    LOG("Ready.");
    LOG("Try:");
    LOG("  curl \"http://ESP_IP/api/led?state=on\"");
    LOG("  curl \"http://ESP_IP/api/led?level=1\"");

}
