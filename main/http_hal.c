#include "http_hal.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "HTTP_HAL";

/**
 * Internal structure of the HTTP HAL instance.
 */
struct http_hal_s {
    httpd_handle_t      server;
    http_hal_config_t   cfg;

    // Endpoints registered before server start are stored here and registered during startup.
    httpd_uri_t        *uris;
    size_t              uris_len;
    size_t              uris_cap;
};

static esp_err_t ensure_capacity(http_hal_t *h, size_t need)
{
    if (h->uris_cap >= need) return ESP_OK;

    size_t new_cap = (h->uris_cap == 0) ? 4 : h->uris_cap * 2;
    while (new_cap < need) new_cap *= 2;

    httpd_uri_t *p = (httpd_uri_t*)realloc(h->uris, new_cap * sizeof(httpd_uri_t));
    if (!p) return ESP_ERR_NO_MEM;

    h->uris = p;
    h->uris_cap = new_cap;
    return ESP_OK;
}

esp_err_t http_hal_init(http_hal_t **out, const http_hal_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(out && cfg, ESP_ERR_INVALID_ARG, TAG, "bad args");

    http_hal_t *h = (http_hal_t*)calloc(1, sizeof(http_hal_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "calloc failed");

    h->server = NULL;
    h->cfg = *cfg;

    *out = h;
    return ESP_OK;
}

static void fill_httpd_config(const http_hal_config_t *in, httpd_config_t *out)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    *out = cfg;

    if (in->port > 0) out->server_port = in->port;
    out->lru_purge_enable = in->lru_purge_enable;

    if (in->max_uri_handlers > 0) out->max_uri_handlers = in->max_uri_handlers;
}

esp_err_t http_hal_start(http_hal_t *h)
{
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h null");
    if (h->server) return ESP_OK;

    httpd_config_t cfg;
    fill_httpd_config(&h->cfg, &cfg);

#if CONFIG_IDF_TARGET_LINUX
    // if user didn't specify a port, use 8080 instead of default 80 to avoid permission issues on Linux
#endif

    ESP_LOGI(TAG, "Starting server on port: %d", cfg.server_port);

    esp_err_t err = httpd_start(&h->server, &cfg);
    ESP_RETURN_ON_ERROR(err, TAG, "httpd_start failed");

    // endpoints registered before start
    for (size_t i = 0; i < h->uris_len; i++) {
        err = httpd_register_uri_handler(h->server, &h->uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed registering URI %s (method %d): %s",
                     h->uris[i].uri, (int)h->uris[i].method, esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

esp_err_t http_hal_stop(http_hal_t *h)
{
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "h null");
    if (!h->server) return ESP_OK;

    ESP_LOGI(TAG, "Stopping server");
    esp_err_t err = httpd_stop(h->server);
    if (err == ESP_OK) {
        h->server = NULL;
    }
    return err;
}

void http_hal_deinit(http_hal_t *h)
{
    if (!h) return;

    (void)http_hal_stop(h);

    free(h->uris);
    free(h);
}

esp_err_t http_hal_register_endpoint(http_hal_t *h, const http_hal_endpoint_t *ep)
{
    ESP_RETURN_ON_FALSE(h && ep && ep->uri && ep->handler, ESP_ERR_INVALID_ARG, TAG, "bad args");

    ESP_RETURN_ON_ERROR(ensure_capacity(h, h->uris_len + 1), TAG, "ensure capacity failed");

    httpd_uri_t u = {
        .uri      = ep->uri,
        .method   = ep->method,
        .handler  = ep->handler,
        .user_ctx = ep->user_ctx
    };

    h->uris[h->uris_len++] = u;

    // if server already started, register immediately
    if (h->server) {
        esp_err_t err = httpd_register_uri_handler(h->server, &h->uris[h->uris_len - 1]);
        ESP_RETURN_ON_ERROR(err, TAG, "register uri failed");
    }
    return ESP_OK;
}

esp_err_t http_hal_unregister_endpoint(http_hal_t *h, const char *uri, httpd_method_t method)
{
    ESP_RETURN_ON_FALSE(h && uri, ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_RETURN_ON_FALSE(h->server, ESP_ERR_INVALID_STATE, TAG, "server not started");

    // For compatibility with older IDF versions, use httpd_unregister_uri if method is HTTP_ANY
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    return httpd_unregister_uri_handler(h->server, uri, method);
#else
    (void)method;
    httpd_unregister_uri(h->server, uri);
    return ESP_OK;
#endif
}

httpd_handle_t http_hal_native_handle(http_hal_t *h)
{
    return h ? h->server : NULL;
}

esp_err_t http_hal_send_json(httpd_req_t *req, int status_code, const char *json)
{
    ESP_RETURN_ON_FALSE(req && json, ESP_ERR_INVALID_ARG, TAG, "bad args");

    // status code -> string
    char status[32];
    snprintf(status, sizeof(status), "%d", status_code);
    httpd_resp_set_status(req, status);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t http_hal_send_err(httpd_req_t *req, int status_code, const char *msg)
{
    ESP_RETURN_ON_FALSE(req && msg, ESP_ERR_INVALID_ARG, TAG, "bad args");

    // map common status codes to httpd_resp_send_err, otherwise fallback to generic JSON error response
    if (status_code == 400) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, msg);
    if (status_code == 401) return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, msg);
    if (status_code == 404) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, msg);
    if (status_code == 413) return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, msg);
    if (status_code == 500) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);

    // fallback to generic JSON error response
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    return http_hal_send_json(req, status_code, buf);
}
