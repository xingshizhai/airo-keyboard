// WiFi 连接管理头文件

#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi 连接状态
 */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_GOT_IP,
    WIFI_STATE_DISCONNECTING,
    WIFI_STATE_ERROR
} wifi_state_t;

/**
 * @brief 初始化 WiFi 模块
 * @param ssid WiFi SSID
 * @param password WiFi 密码
 * @return esp_err_t
 */
esp_err_t wifi_init(const char *ssid, const char *password);

/**
 * @brief 启动 WiFi 连接
 * @return esp_err_t
 */
esp_err_t wifi_connect(void);

/**
 * @brief 断开 WiFi 连接
 * @return esp_err_t
 */
esp_err_t wifi_disconnect(void);

/**
 * @brief 获取当前 WiFi 连接状态
 * @return wifi_state_t
 */
wifi_state_t wifi_get_state(void);

/**
 * @brief 检查 WiFi 是否已连接并获取 IP
 * @return true 已连接并获取 IP，false 未连接
 */
bool wifi_is_connected(void);

/**
 * @brief 获取 IP 地址字符串
 * @param ip_buffer 缓冲区，至少 16 字节
 * @return esp_err_t
 */
esp_err_t wifi_get_ip_address(char *ip_buffer);

/**
 * @brief 获取 SSID 字符串
 * @param ssid_buffer 缓冲区，至少 32 字节
 * @return esp_err_t
 */
esp_err_t wifi_get_ssid(char *ssid_buffer);

/**
 * @brief 获取当前 RSSI (信号强度)
 * @param rssi 输出参数，RSSI 值 (dBm)
 * @return esp_err_t
 */
esp_err_t wifi_get_rssi(int8_t *rssi);

/**
 * @brief 反初始化 WiFi 模块
 * @return esp_err_t
 */
esp_err_t kb_wifi_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
