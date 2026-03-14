// USB HID keyboard device implementation for ESP-IDF 6.x

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
#include "usb_hid.h"

static const char *TAG = "usb_hid";

#define HID_ITF_NUM 0
#define HID_REPORT_ID_KEYBOARD HID_ITF_PROTOCOL_KEYBOARD
#define HID_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define HID_RELEASE_DELAY_MS 8
#define HID_READY_TIMEOUT_MS 120
#define HID_RELEASE_RETRY_MS 120
#define HID_CUSTOM_PID 0x4B54
#define HID_ENTER_TOKEN ((char)0x1F)

static bool s_release_pending = false;

static bool wait_hid_ready(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start) <= timeout_ticks) {
        if (tud_mounted() && tud_hid_ready()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return false;
}

static void service_pending_release(void)
{
    if (!s_release_pending) {
        return;
    }

    if (!tud_mounted() || !tud_hid_ready()) {
        return;
    }

    if (tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, 0, NULL)) {
        s_release_pending = false;
        ESP_LOGD(TAG, "Pending key release sent");
    }
}

static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD)),
};
static const char *s_hid_string_descriptor[] = {
    (const char[]){0x09, 0x04},
    "Espressif Systems",
    "ESP32 K54 Macro Keyboard",
    "K54-20260307",
    "Custom Keyboard HID",
};

static const tusb_desc_device_t s_hid_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = TINYUSB_ESPRESSIF_VID,
    .idProduct = HID_CUSTOM_PID,
    .bcdDevice = 0x0200,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t s_hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, HID_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(HID_ITF_NUM, 4, false, sizeof(s_hid_report_descriptor), 0x81, 16, 10),
};

static bool ascii_to_hid_keycode(char ch, uint8_t *keycode, uint8_t *modifier)
{
    if (!keycode || !modifier) {
        return false;
    }

    *keycode = HID_KEY_NONE;
    *modifier = 0;

    if (ch >= 'a' && ch <= 'z') {
        *keycode = (uint8_t)(HID_KEY_A + (ch - 'a'));
        return true;
    }

    if (ch >= 'A' && ch <= 'Z') {
        *keycode = (uint8_t)(HID_KEY_A + (ch - 'A'));
        *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        return true;
    }

    if (ch >= '1' && ch <= '9') {
        *keycode = (uint8_t)(HID_KEY_1 + (ch - '1'));
        return true;
    }

    switch (ch) {
        case '\n':
        case '\r':
        case HID_ENTER_TOKEN:
            *keycode = HID_KEY_ENTER;
            return true;
        case '0':
            *keycode = HID_KEY_0;
            return true;
        case ' ':
            *keycode = HID_KEY_SPACE;
            return true;
        case '.':
            *keycode = HID_KEY_PERIOD;
            return true;
        case ',':
            *keycode = HID_KEY_COMMA;
            return true;
        case ';':
            *keycode = HID_KEY_SEMICOLON;
            return true;
        case ':':
            *keycode = HID_KEY_SEMICOLON;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '!':
            *keycode = HID_KEY_1;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '?':
            *keycode = HID_KEY_SLASH;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '@':
            *keycode = HID_KEY_2;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '#':
            *keycode = HID_KEY_3;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '$':
            *keycode = HID_KEY_4;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '%':
            *keycode = HID_KEY_5;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '^':
            *keycode = HID_KEY_6;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '&':
            *keycode = HID_KEY_7;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '*':
            *keycode = HID_KEY_8;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '(':
            *keycode = HID_KEY_9;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case ')':
            *keycode = HID_KEY_0;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '-':
            *keycode = HID_KEY_MINUS;
            return true;
        case '_':
            *keycode = HID_KEY_MINUS;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '=':
            *keycode = HID_KEY_EQUAL;
            return true;
        case '+':
            *keycode = HID_KEY_EQUAL;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '/':
            *keycode = HID_KEY_SLASH;
            return true;
        case '\\':
            *keycode = HID_KEY_BACKSLASH;
            return true;
        case '|':
            *keycode = HID_KEY_BACKSLASH;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '[':
            *keycode = HID_KEY_BRACKET_LEFT;
            return true;
        case '{':
            *keycode = HID_KEY_BRACKET_LEFT;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case ']':
            *keycode = HID_KEY_BRACKET_RIGHT;
            return true;
        case '}':
            *keycode = HID_KEY_BRACKET_RIGHT;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '\'':
            *keycode = HID_KEY_APOSTROPHE;
            return true;
        case '"':
            *keycode = HID_KEY_APOSTROPHE;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        case '`':
            *keycode = HID_KEY_GRAVE;
            return true;
        case '~':
            *keycode = HID_KEY_GRAVE;
            *modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            return true;
        default:
            return false;
    }
}

static esp_err_t usb_hid_keyboard_report_with_modifier(uint8_t modifier, uint8_t keycode[6])
{
    if (!keycode) {
        return ESP_ERR_INVALID_ARG;
    }

    service_pending_release();

    if (!tud_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!wait_hid_ready(HID_READY_TIMEOUT_MS)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, modifier, keycode)) {
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(HID_RELEASE_DELAY_MS));

    if (wait_hid_ready(HID_RELEASE_RETRY_MS) && tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, 0, NULL)) {
        s_release_pending = false;
        return ESP_OK;
    }

    s_release_pending = true;
    ESP_LOGW(TAG, "Key release deferred (USB busy), will retry");
    return ESP_OK;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

esp_err_t usb_hid_keyboard_init(void)
{
    ESP_LOGI(TAG, "Initializing USB HID keyboard device");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &s_hid_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = s_hid_configuration_descriptor;
    tusb_cfg.descriptor.string = s_hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(s_hid_string_descriptor) / sizeof(s_hid_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_hid_configuration_descriptor;
#endif

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB HID keyboard initialized successfully (connect board USB DEV port to PC)");
    return ESP_OK;
}

esp_err_t usb_hid_keyboard_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing USB HID keyboard device");
    return tinyusb_driver_uninstall();
}

esp_err_t usb_hid_keyboard_report(uint8_t keycode[6])
{
    return usb_hid_keyboard_report_with_modifier(0, keycode);
}

esp_err_t usb_hid_keyboard_send_ascii(char ch)
{
    uint8_t hid_key = HID_KEY_NONE;
    uint8_t modifier = 0;
    uint8_t report[6] = {0};

    if (!ascii_to_hid_keycode(ch, &hid_key, &modifier)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    report[0] = hid_key;
    return usb_hid_keyboard_report_with_modifier(modifier, report);
}

esp_err_t usb_hid_keyboard_send_combo(uint8_t modifier, uint8_t keycode)
{
    uint8_t report[6] = {0};
    
    if (keycode == HID_KEY_NONE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    report[0] = keycode;
    ESP_LOGI(TAG, "Sending combo: modifier=0x%02X, keycode=0x%02X", modifier, keycode);
    return usb_hid_keyboard_report_with_modifier(modifier, report);
}

bool usb_hid_is_connected(void)
{
    service_pending_release();
    return tud_mounted();
}
