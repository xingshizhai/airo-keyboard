// 主菜单UI头文件

#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 菜单项枚举
typedef enum {
    UI_MENU_ITEM_COMBO_TAB_ALT = 0,   // Tab+Alt组合键
    UI_MENU_ITEM_COMBO_FN_Q,          // Fn+Q组合键
    UI_MENU_ITEM_VIRTUAL_KEYBOARD,    // 虚拟键盘
    UI_MENU_ITEM_WIFI_INFO,           // Wi-Fi信息
    UI_MENU_ITEM_COUNT
} ui_menu_item_t;

// 菜单导航事件
typedef enum {
    UI_MENU_NAV_UP,
    UI_MENU_NAV_DOWN,
    UI_MENU_NAV_SELECT
} ui_menu_nav_event_t;

/**
 * @brief 初始化主菜单UI
 */
esp_err_t ui_menu_init(void);

/**
 * @brief 反初始化主菜单UI
 */
esp_err_t ui_menu_deinit(void);

/**
 * @brief 获取当前选中的菜单项
 */
ui_menu_item_t ui_menu_get_selected_item(void);

/**
 * @brief 处理菜单导航事件
 */
esp_err_t ui_menu_handle_event(ui_menu_nav_event_t nav_event);

/**
 * @brief 刷新菜单显示
 */
esp_err_t ui_menu_refresh_display(void);

/**
 * @brief 显示Wi-Fi信息页面
 */
esp_err_t ui_menu_show_wifi_info(void);

/**
 * @brief 设置WiFi连接状态
 * @param connected 是否已连接
 * @param ip_address IP地址字符串，可为NULL
 */
void ui_menu_set_wifi_status(bool connected, const char *ip_address);

#ifdef __cplusplus
}
#endif

#endif // UI_MENU_H