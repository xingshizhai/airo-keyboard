// 虚拟键盘UI头文件

#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_KEYBOARD_NAV_UP = 0,
    UI_KEYBOARD_NAV_DOWN,
    UI_KEYBOARD_NAV_LEFT,
    UI_KEYBOARD_NAV_RIGHT,
    UI_KEYBOARD_NAV_SELECT,
} ui_keyboard_nav_event_t;

/**
 * @brief 初始化UI键盘
 */
esp_err_t ui_keyboard_init(void);

/**
 * @brief 反初始化UI键盘
 */
esp_err_t ui_keyboard_deinit(void);

/**
 * @brief 获取当前选中的字符
 */
char ui_keyboard_get_selected_char(void);

/**
 * @brief 处理按键事件
 */
esp_err_t ui_keyboard_handle_event(ui_keyboard_nav_event_t nav_event);

/**
 * @brief 刷新键盘显示
 */
esp_err_t ui_keyboard_refresh_display(void);

/**
 * @brief 切换到下一页
 */
void ui_keyboard_next_page(void);

/**
 * @brief 切换到上一页
 */
void ui_keyboard_prev_page(void);

#ifdef __cplusplus
}
#endif

#endif // UI_KEYBOARD_H
