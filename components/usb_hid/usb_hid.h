// USB HID键盘设备头文件

#ifndef USB_HID_H
#define USB_HID_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化USB HID键盘设备
 */
esp_err_t usb_hid_keyboard_init(void);

/**
 * @brief 反初始化USB HID键盘设备
 */
esp_err_t usb_hid_keyboard_deinit(void);

/**
 * @brief 发送键盘报告
 * @param keycode 键盘键码数组（6字节）
 */
esp_err_t usb_hid_keyboard_report(uint8_t keycode[6]);

/**
 * @brief 发送ASCII字符（组件内部做HID映射）
 */
esp_err_t usb_hid_keyboard_send_ascii(char ch);

/**
 * @brief 发送组合键（modifier + keycode）
 * @param modifier 修饰键掩码（Ctrl/Shift/Alt/GUI）
 * @param keycode HID键码
 * @note 例如：Tab+Alt = KEYBOARD_MODIFIER_LEFTALT | keycode=HID_KEY_TAB
 */
esp_err_t usb_hid_keyboard_send_combo(uint8_t modifier, uint8_t keycode);

/**
 * @brief 检查USB HID是否已连接
 * @return true 已连接，false 未连接
 */
bool usb_hid_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // USB_HID_H
