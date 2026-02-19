#include "wifi.h"

static const char *TAG = "WIFI";

static esp_netif_t *s_netif_sta;

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT      = BIT1;

static int s_retry_num = 0;

static void stdio_prepare(void)
{
    /* Make stdin/stdout unbuffered to work nicely with idf.py monitor */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void strip_newline(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_WIFI_MAX_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Retry to connect to AP (%d/%d)...", s_retry_num, CONFIG_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        return;
    }
}


esp_err_t wifi_init_connection(void){
    esp_err_t ret;

    ret =esp_netif_init();
    if (ret != ESP_OK) return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) return ret;

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) return ESP_ERR_NO_MEM;

    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}

esp_err_t wifi_connect_sta(void)
{
    /* Assumes esp_netif_init() and esp_event_loop_create_default() were called in app_main */

    char ssid[33] = {0};
    char pwd[65]  = {0};

#if CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN
    stdio_prepare();
    printf("Enter Wi-Fi SSID:\n");
    if (!fgets(ssid, sizeof(ssid), stdin)) {
        return ESP_FAIL;
    }
    strip_newline(ssid);

    printf("Enter Wi-Fi Password:\n");
    if (!fgets(pwd, sizeof(pwd), stdin)) {
        return ESP_FAIL;
    }
    strip_newline(pwd);
#else
    strncpy(ssid, CONFIG_WIFI_SSID, sizeof(ssid) - 1);
    strncpy(pwd,  CONFIG_WIFI_PASS, sizeof(pwd) - 1);
#endif

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pwd, sizeof(wifi_config.sta.password) - 1);

    /* If you need WPA3 or open networks, adjust here */
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);

    esp_err_t ret = ESP_FAIL;
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP");
        ret = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP");
        ret = ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

    return ret;
}

esp_netif_t *wifi_get_netif_sta(void)
{
    return s_netif_sta;
}

esp_err_t wifi_disable_powersave(void)
{
    return esp_wifi_set_ps(WIFI_PS_NONE);
}
