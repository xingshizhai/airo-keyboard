// WiFi 连接管理实现 - 简化版本

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "nvs_flash.h"
#include "lwip/inet.h"

#include "wifi.h"

static const char *TAG = "wifi";

// 内部状态
static struct {
    wifi_state_t state;
    char ssid[32];
    char password[64];
    esp_netif_t *netif;
    bool initialized;
    bool auto_reconnect;
    char ip_address[16];
} s_wifi_ctx = {
    .state = WIFI_STATE_DISCONNECTED,
    .ssid = "DeepSeek",
    .password = "Wxxncbdmm1804",
    .netif = NULL,
    .initialized = false,
    .auto_reconnect = true,
    .ip_address = {0}
};

// WiFi 事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                s_wifi_ctx.state = WIFI_STATE_CONNECTING;
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                {
                    wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
                    ESP_LOGI(TAG, "Connected to AP: %s, channel: %d", event->ssid, event->channel);
                    s_wifi_ctx.state = WIFI_STATE_CONNECTED;
                }
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                {
                    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                    ESP_LOGW(TAG, "Disconnected from AP: %s, reason: %d", 
                             event->ssid, event->reason);
                    s_wifi_ctx.state = WIFI_STATE_DISCONNECTED;
                    memset(s_wifi_ctx.ip_address, 0, sizeof(s_wifi_ctx.ip_address));
                    
                    if (s_wifi_ctx.auto_reconnect && s_wifi_ctx.initialized) {
                        ESP_LOGI(TAG, "Auto-reconnecting to WiFi...");
                        esp_wifi_connect();
                    }
                }
                break;

            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "WiFi STA stopped");
                s_wifi_ctx.state = WIFI_STATE_DISCONNECTED;
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                {
                    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                    esp_ip4addr_ntoa(&event->ip_info.ip, s_wifi_ctx.ip_address, sizeof(s_wifi_ctx.ip_address));
                    ESP_LOGI(TAG, "Got IP: %s", s_wifi_ctx.ip_address);
                    s_wifi_ctx.state = WIFI_STATE_GOT_IP;
                }
                break;

            case IP_EVENT_STA_LOST_IP:
                ESP_LOGW(TAG, "Lost IP address");
                memset(s_wifi_ctx.ip_address, 0, sizeof(s_wifi_ctx.ip_address));
                s_wifi_ctx.state = WIFI_STATE_CONNECTED;
                break;

            default:
                break;
        }
    }
}

// 公共 API 实现
esp_err_t wifi_init(const char *ssid, const char *password)
{
    if (s_wifi_ctx.initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi with SSID: %s", ssid ? ssid : "default");

    // 复制配置
    if (ssid != NULL) {
        strncpy(s_wifi_ctx.ssid, ssid, sizeof(s_wifi_ctx.ssid) - 1);
        s_wifi_ctx.ssid[sizeof(s_wifi_ctx.ssid) - 1] = '\0';
    }
    if (password != NULL) {
        strncpy(s_wifi_ctx.password, password, sizeof(s_wifi_ctx.password) - 1);
        s_wifi_ctx.password[sizeof(s_wifi_ctx.password) - 1] = '\0';
    }

    // 初始化 TCP/IP 栈
    ESP_ERROR_CHECK(esp_netif_init());
    
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建 STA 网络接口
    s_wifi_ctx.netif = esp_netif_create_default_wifi_sta();
    if (s_wifi_ctx.netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }

    // 初始化 WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // 设置 WiFi 模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 配置 WiFi
    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char *)wifi_cfg.sta.ssid, s_wifi_ctx.ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, s_wifi_ctx.password, sizeof(wifi_cfg.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_ctx.initialized = true;
    s_wifi_ctx.state = WIFI_STATE_DISCONNECTED;
    
    ESP_LOGI(TAG, "WiFi initialized");
    
    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    if (!s_wifi_ctx.initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_FAIL;
    }

    if (s_wifi_ctx.state == WIFI_STATE_CONNECTING || 
        s_wifi_ctx.state == WIFI_STATE_CONNECTED || 
        s_wifi_ctx.state == WIFI_STATE_GOT_IP) {
        ESP_LOGW(TAG, "WiFi already connecting/connected");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Connecting to WiFi...");
    s_wifi_ctx.state = WIFI_STATE_CONNECTING;
    esp_err_t ret = esp_wifi_connect();
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start connection: %s", esp_err_to_name(ret));
        s_wifi_ctx.state = WIFI_STATE_DISCONNECTED;
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_disconnect(void)
{
    if (!s_wifi_ctx.initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Disconnecting from WiFi...");
    s_wifi_ctx.auto_reconnect = false;
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_ctx.state = WIFI_STATE_DISCONNECTING;
    return ESP_OK;
}

wifi_state_t wifi_get_state(void)
{
    return s_wifi_ctx.state;
}

bool wifi_is_connected(void)
{
    return s_wifi_ctx.state == WIFI_STATE_GOT_IP;
}

esp_err_t wifi_get_ip_address(char *ip_buffer)
{
    if (!ip_buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_wifi_ctx.state != WIFI_STATE_GOT_IP) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    strncpy(ip_buffer, s_wifi_ctx.ip_address, 16);
    return ESP_OK;
}

esp_err_t wifi_get_ssid(char *ssid_buffer)
{
    if (!ssid_buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(s_wifi_ctx.ssid) == 0) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    strncpy(ssid_buffer, s_wifi_ctx.ssid, 32);
    return ESP_OK;
}

esp_err_t wifi_get_rssi(int8_t *rssi)
{
    if (!rssi) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_wifi_ctx.state != WIFI_STATE_CONNECTED && s_wifi_ctx.state != WIFI_STATE_GOT_IP) {
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(ret));
        return ret;
    }

    *rssi = ap_info.rssi;
    return ESP_OK;
}

esp_err_t kb_wifi_deinit(void)
{
    if (!s_wifi_ctx.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi");

    s_wifi_ctx.auto_reconnect = false;

    if (s_wifi_ctx.state == WIFI_STATE_CONNECTED || 
        s_wifi_ctx.state == WIFI_STATE_GOT_IP ||
        s_wifi_ctx.state == WIFI_STATE_CONNECTING) {
        esp_wifi_disconnect();
    }

    esp_wifi_stop();

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);

    if (s_wifi_ctx.netif) {
        esp_netif_destroy_default_wifi(s_wifi_ctx.netif);
        s_wifi_ctx.netif = NULL;
    }

    esp_wifi_deinit();

    s_wifi_ctx.initialized = false;
    s_wifi_ctx.state = WIFI_STATE_DISCONNECTED;
    memset(s_wifi_ctx.ip_address, 0, sizeof(s_wifi_ctx.ip_address));

    ESP_LOGI(TAG, "WiFi deinitialized");
    return ESP_OK;
}
