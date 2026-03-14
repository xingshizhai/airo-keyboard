#ifndef _BSP_ESP32_S3_USB_OTG_EV_H_
#define _BSP_ESP32_S3_USB_OTG_EV_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NULL_RESOURCE = 0,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
    BOARD_SPI3_ID,
    BOARD_LCD_ID,
    BOARD_SDCARD_ID,
    BOARD_BTN_OK_ID,
    BOARD_BTN_UP_ID,
    BOARD_BTN_DW_ID,
    BOARD_BTN_MN_ID,
} board_res_id_t;

typedef void *board_res_handle_t;

typedef enum {
    USB_DEVICE_MODE,
    USB_HOST_MODE,
} usb_mode_t;

#define BOARD_IO_BUTTON_OK 0
#define BOARD_IO_BUTTON_DW 11
#define BOARD_IO_BUTTON_UP 10
#define BOARD_IO_BUTTON_MENU 14

#define BOARD_IO_LED_GREEN 15
#define BOARD_IO_LED_YELLOW 16

#define BOARD_IO_HOST_BOOST_EN 13
#define BOARD_IO_DEV_VBUS_EN 12
#define BOARD_IO_USB_SEL 18
#define BOARD_IO_IDEV_LIMIT_EN 17

esp_err_t iot_board_init(void);
esp_err_t iot_board_deinit(void);
bool iot_board_is_init(void);
board_res_handle_t iot_board_get_handle(board_res_id_t id);
char *iot_board_get_info(void);

esp_err_t iot_board_usb_device_set_power(bool on_off, bool from_battery);
bool iot_board_usb_device_get_power(void);
float iot_board_get_host_voltage(void);
float iot_board_get_battery_voltage(void);
esp_err_t iot_board_usb_set_mode(usb_mode_t mode);
usb_mode_t iot_board_usb_get_mode(void);

esp_err_t iot_board_button_register_cb(board_res_handle_t btn_handle, button_event_t event, button_cb_t cb);
esp_err_t iot_board_button_unregister_cb(board_res_handle_t btn_handle, button_event_t event);

esp_err_t iot_board_led_set_state(int gpio_num, bool turn_on);
esp_err_t iot_board_led_all_set_state(bool turn_on);
esp_err_t iot_board_led_toggle_state(int gpio_num);

esp_err_t iot_board_wifi_init(void);
esp_err_t board_lcd_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *img);

#ifdef __cplusplus
}
#endif

#endif
