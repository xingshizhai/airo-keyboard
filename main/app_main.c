// ESP32 USB keyboard main entry

#include <stdbool.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "bsp_esp32_s3_usb_otg_ev.h"
#include "ui_keyboard.h"
#include "ui_menu.h"
#include "usb_hid.h"
#include "wifi.h"
#include "class/hid/hid.h"  // For HID key codes

static const char *TAG = "app_main";
#define APP_KEY_ENTER_TOKEN ((char)0x1F)

// 应用状态
typedef enum {
    APP_STATE_MENU = 0,           // 主菜单
    APP_STATE_VIRTUAL_KEYBOARD,
    APP_STATE_WIFI_INFO,   // 虚拟键盘
} app_state_t;

static app_state_t s_app_state = APP_STATE_MENU;

typedef enum {
    APP_BTN_UP = 0,
    APP_BTN_DOWN,
    APP_BTN_LEFT,
    APP_BTN_RIGHT,
    APP_BTN_OK,
    APP_BTN_MENU_LONG_PRESS,  // 长按返回主菜单
} app_btn_event_t;

static QueueHandle_t s_input_queue = NULL;

static void push_button_event(app_btn_event_t event)
{
    if (!s_input_queue) {
        return;
    }

    (void)xQueueSend(s_input_queue, &event, 0);
}

static void button_up_single_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_UP);
}

static void button_down_single_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_DOWN);
}

static void button_menu_single_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_RIGHT);
}

static void button_menu_double_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_LEFT);
}

static void button_ok_single_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_OK);
}

static void button_menu_long_press_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    push_button_event(APP_BTN_MENU_LONG_PRESS);
}

static void keyboard_send_selected_key(void)
{
    if (!usb_hid_is_connected()) {
        ESP_LOGW(TAG, "USB not connected, cannot send key");
        return;
    }

    char selected = ui_keyboard_get_selected_char();
    esp_err_t ret = usb_hid_keyboard_send_ascii(selected);
    if (ret != ESP_OK) {
        if (selected == APP_KEY_ENTER_TOKEN) {
            ESP_LOGW(TAG, "Failed to send key ENTER: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGW(TAG, "Failed to send key '%c': %s", selected, esp_err_to_name(ret));
        }
        return;
    }

    if (selected == APP_KEY_ENTER_TOKEN) {
        ESP_LOGI(TAG, "Key sent: ENTER");
    } else {
        ESP_LOGI(TAG, "Key sent: '%c'", selected);
    }
}

static void send_menu_combo(uint8_t modifier, uint8_t keycode, const char *combo_name)
{
    if (!usb_hid_is_connected()) {
        ESP_LOGW(TAG, "USB not connected, skip combo: %s", combo_name);
        return;
    }

    esp_err_t ret = usb_hid_keyboard_send_combo(modifier, keycode);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Combo sent: %s", combo_name);
        return;
    }

    ESP_LOGW(TAG, "Failed to send combo %s: %s", combo_name, esp_err_to_name(ret));
}

static void log_usb_mount_change(bool *last_usb_connected)
{
    bool connected = usb_hid_is_connected();
    if (connected == *last_usb_connected) {
        return;
    }

    *last_usb_connected = connected;
    ESP_LOGI(TAG, "USB mounted changed: %s", connected ? "yes" : "no");
}

static void update_wifi_status_to_ui(void)
{
    static bool last_wifi_connected = false;
    static char last_ip_address[16] = "";
    
    bool connected = wifi_is_connected();
    char ip_address[16] = "";
    
    if (connected) {
        esp_err_t ret = wifi_get_ip_address(ip_address);
        if (ret != ESP_OK) {
            ip_address[0] = '\0';
        }
    }
    
    if (connected != last_wifi_connected || strcmp(ip_address, last_ip_address) != 0) {
        last_wifi_connected = connected;
        strncpy(last_ip_address, ip_address, sizeof(last_ip_address) - 1);
        last_ip_address[sizeof(last_ip_address) - 1] = '\0';
        
        ui_menu_set_wifi_status(connected, ip_address[0] != '\0' ? ip_address : NULL);
        ESP_LOGI(TAG, "WiFi status updated: connected=%d, ip=%s", connected, ip_address);
    }
}

static void handle_menu_selection(void)
{
    ui_menu_item_t item = ui_menu_get_selected_item();

    switch (item) {
        case UI_MENU_ITEM_COMBO_TAB_ALT:
            ESP_LOGI(TAG, "Executing: Tab+Alt combo");
            send_menu_combo(KEYBOARD_MODIFIER_LEFTALT, HID_KEY_TAB, "Tab+Alt");
            break;
            
        case UI_MENU_ITEM_COMBO_FN_Q:
            // 注意：Fn键不是标准HID键，用Win+Q代替
            ESP_LOGI(TAG, "Executing: Win+Q combo (Fn not available in HID)");
            send_menu_combo(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_Q, "Win+Q");
            break;
            
        case UI_MENU_ITEM_VIRTUAL_KEYBOARD:
            ESP_LOGI(TAG, "Entering virtual keyboard mode");
            s_app_state = APP_STATE_VIRTUAL_KEYBOARD;
            ESP_ERROR_CHECK(ui_keyboard_init());
            break;
            
        case UI_MENU_ITEM_WIFI_INFO:
            ESP_LOGI(TAG, "Entering Wi-Fi info mode");
            s_app_state = APP_STATE_WIFI_INFO;
            ESP_ERROR_CHECK(ui_menu_show_wifi_info());
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown menu item: %d", item);
            break;
    }
}

static void return_to_menu(void)
{
    ESP_LOGI(TAG, "Returning to main menu");
    
    // 反初始化键盘UI（如果在键盘模式）
    if (s_app_state == APP_STATE_VIRTUAL_KEYBOARD) {
        (void)ui_keyboard_deinit();
    }
    
    // 切换到菜单状态
    s_app_state = APP_STATE_MENU;
    
    // 初始化菜单UI
    ESP_ERROR_CHECK(ui_menu_init());
}

static void app_register_button_callbacks(void)
{
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_OK_ID), BUTTON_SINGLE_CLICK, button_ok_single_click_cb);
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_UP_ID), BUTTON_SINGLE_CLICK, button_up_single_click_cb);
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_DW_ID), BUTTON_SINGLE_CLICK, button_down_single_click_cb);
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_MN_ID), BUTTON_SINGLE_CLICK, button_menu_single_click_cb);
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_MN_ID), BUTTON_DOUBLE_CLICK, button_menu_double_click_cb);
    iot_board_button_register_cb(iot_board_get_handle(BOARD_BTN_MN_ID), BUTTON_LONG_PRESS_START, button_menu_long_press_cb);
}

static void app_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP32 USB keyboard");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_input_queue = xQueueCreate(8, sizeof(app_btn_event_t));
    ESP_ERROR_CHECK(s_input_queue ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(iot_board_init());
    ESP_ERROR_CHECK(iot_board_usb_set_mode(USB_DEVICE_MODE));
    ESP_ERROR_CHECK(iot_board_usb_device_set_power(true, false));

    ESP_ERROR_CHECK(usb_hid_keyboard_init());
    
    // 初始化WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(wifi_init("DeepSeek", "Wxxncbdmm1804"));
    ESP_ERROR_CHECK(wifi_connect());
    
    // 初始化时显示主菜单
    s_app_state = APP_STATE_MENU;
    ESP_ERROR_CHECK(ui_menu_init());

    app_register_button_callbacks();
}

static void app_task(void *pvParameters)
{
    (void)pvParameters;
    app_init();

    bool last_usb_connected = usb_hid_is_connected();
    ESP_LOGI(TAG, "USB mounted: %s", last_usb_connected ? "yes" : "no");
    ESP_LOGI(TAG, "=== MAIN MENU ===");
    ESP_LOGI(TAG, "Controls: UP/DOWN to navigate, OK to select, MENU long-press to return");

    while (1) {
        app_btn_event_t event;
        if (xQueueReceive(s_input_queue, &event, pdMS_TO_TICKS(200)) != pdTRUE) {
            log_usb_mount_change(&last_usb_connected);
            update_wifi_status_to_ui();
            continue;
        }

        // 处理MENU长按 - 返回主菜单（所有状态通用）
        if (event == APP_BTN_MENU_LONG_PRESS) {
            if (s_app_state != APP_STATE_MENU) {
                return_to_menu();
            }
            continue;
        }

        // 根据当前状态处理事件
        if (s_app_state == APP_STATE_MENU) {
            // ===== 主菜单状态 =====
            switch (event) {
                case APP_BTN_UP:
                    (void)ui_menu_handle_event(UI_MENU_NAV_UP);
                    break;
                case APP_BTN_DOWN:
                    (void)ui_menu_handle_event(UI_MENU_NAV_DOWN);
                    break;
                case APP_BTN_OK:
                    // 选择菜单项并执行
                    handle_menu_selection();
                    break;
                default:
                    break;
            }
            
        } else if (s_app_state == APP_STATE_WIFI_INFO) {
            // ===== Wi-Fi Info 状态 =====
            // MENU 长按返回主菜单
            switch (event) {
                case APP_BTN_OK:
                    // 刷新 WiFi 信息
                    ESP_LOGI(TAG, "Refreshing WiFi info...");
                    ui_menu_show_wifi_info();
                    break;
                default:
                    break;
            }

        } else if (s_app_state == APP_STATE_VIRTUAL_KEYBOARD) {
            // ===== 虚拟键盘状态 =====
            switch (event) {
                case APP_BTN_UP:
                    (void)ui_keyboard_handle_event(UI_KEYBOARD_NAV_UP);
                    break;
                case APP_BTN_DOWN:
                    (void)ui_keyboard_handle_event(UI_KEYBOARD_NAV_DOWN);
                    break;
                case APP_BTN_LEFT:
                    // MENU double-click: previous page
                    ui_keyboard_prev_page();
                    (void)ui_keyboard_refresh_display();
                    break;
                case APP_BTN_RIGHT:
                    // MENU single-click: next page
                    ui_keyboard_next_page();
                    (void)ui_keyboard_refresh_display();
                    break;
                case APP_BTN_OK:
                    // OK single-click: send selected character
                    keyboard_send_selected_key();
                    break;
                default:
                    break;
            }
        }

        log_usb_mount_change(&last_usb_connected);
        update_wifi_status_to_ui();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 USB Macro Keyboard ===");
    xTaskCreate(app_task, "app_task", 8192, NULL, 5, NULL);
}
