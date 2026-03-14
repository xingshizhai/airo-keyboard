#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_check.h"
#include "esp_log.h"
#include "button_gpio.h"
#include "bsp_esp32_s3_usb_otg_ev.h"

static const char *TAG = "bsp_otg";

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define BOARD_LCD_SPI_HOST SPI3_HOST
#define BOARD_LCD_SPI_MOSI 7
#define BOARD_LCD_SPI_CLK 6
#define BOARD_LCD_SPI_CS 5
#define BOARD_LCD_DC 4
#define BOARD_LCD_RST 8
#define BOARD_LCD_BACKLIGHT 9

#define BOARD_LCD_H_RES 240
#define BOARD_LCD_V_RES 240
#define BOARD_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define BOARD_LCD_DRAW_BUF_HEIGHT 24
#define BOARD_LCD_SHOW_BOOT_PATTERN 1
#define BOARD_LCD_BOOT_PATTERN_MS 500

static bool s_board_is_init = false;
static bool s_board_lcd_is_init = false;
static button_handle_t s_btn_ok = NULL;
static button_handle_t s_btn_up = NULL;
static button_handle_t s_btn_dw = NULL;
static button_handle_t s_btn_menu = NULL;
static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_panel_io_handle_t s_lcd_io = NULL;

static const int s_led_io[] = {
    BOARD_IO_LED_GREEN,
    BOARD_IO_LED_YELLOW,
};

static esp_err_t board_lcd_init(void);
static esp_err_t board_lcd_deinit(void);
static esp_err_t board_lcd_clear(uint16_t color);
static esp_err_t board_lcd_draw_boot_pattern(void);

static void usb_otg_route_to_internal_phy(void)
{
#if CONFIG_IDF_TARGET_ESP32S3
    uint32_t *usb_phy_sel_reg = (uint32_t *)(0x60008000 + 0x120);
    *usb_phy_sel_reg |= BIT(19) | BIT(20);
#endif
}

static esp_err_t create_gpio_button(int gpio_num, button_handle_t *out_handle)
{
    button_config_t btn_cfg = {
        .long_press_time = 1000,
        .short_press_time = 180,
    };

    button_gpio_config_t gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };

    return iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, out_handle);
}

static esp_err_t board_lcd_clear(uint16_t color)
{
    if (!s_board_lcd_is_init || !s_lcd_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t line_buf[BOARD_LCD_H_RES];
    for (size_t i = 0; i < BOARD_LCD_H_RES; i++) {
        line_buf[i] = color;
    }

    for (uint16_t y = 0; y < BOARD_LCD_V_RES; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, y, BOARD_LCD_H_RES, y + 1, line_buf);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t board_lcd_draw_boot_pattern(void)
{
    if (!s_board_lcd_is_init || !s_lcd_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint16_t bands[] = {
        0xF800, // red
        0x07E0, // green
        0x001F, // blue
        0xFFFF, // white
    };
    uint16_t line_buf[BOARD_LCD_H_RES];
    const uint16_t band_height = BOARD_LCD_V_RES / (sizeof(bands) / sizeof(bands[0]));

    for (size_t b = 0; b < sizeof(bands) / sizeof(bands[0]); b++) {
        for (size_t i = 0; i < BOARD_LCD_H_RES; i++) {
            line_buf[i] = bands[b];
        }

        uint16_t y_start = b * band_height;
        uint16_t y_end = (b == (sizeof(bands) / sizeof(bands[0]) - 1)) ? BOARD_LCD_V_RES : (uint16_t)((b + 1) * band_height);
        for (uint16_t y = y_start; y < y_end; y++) {
            esp_err_t ret = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, y, BOARD_LCD_H_RES, y + 1, line_buf);
            if (ret != ESP_OK) {
                return ret;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t board_lcd_init(void)
{
    if (s_board_lcd_is_init) {
        return ESP_OK;
    }

    bool spi_initialized_here = false;

    const gpio_config_t bk_gpio_cfg = {
        .pin_bit_mask = (1ULL << BOARD_LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_cfg), TAG, "LCD backlight GPIO config failed");
    gpio_set_level(BOARD_LCD_BACKLIGHT, 0);

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = BOARD_LCD_SPI_CLK,
        .mosi_io_num = BOARD_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUF_HEIGHT * sizeof(uint16_t),
    };

    esp_err_t ret = spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        spi_initialized_here = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "LCD SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BOARD_LCD_DC,
        .cs_gpio_num = BOARD_LCD_SPI_CS,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST, &io_cfg, &s_lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD panel IO create failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ret = esp_lcd_new_panel_st7789(s_lcd_io, &panel_cfg, &s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD panel create failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(s_lcd_panel), fail, TAG, "LCD panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(s_lcd_panel), fail, TAG, "LCD panel init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_panel, true), fail, TAG, "LCD color invert failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_swap_xy(s_lcd_panel, false), fail, TAG, "LCD swap xy failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_mirror(s_lcd_panel, false, false), fail, TAG, "LCD mirror failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel, true), fail, TAG, "LCD display on failed");

    gpio_set_level(BOARD_LCD_BACKLIGHT, 1);
    vTaskDelay(pdMS_TO_TICKS(30));

    s_board_lcd_is_init = true;
    ESP_GOTO_ON_ERROR(board_lcd_clear(0x0000), fail, TAG, "LCD clear failed");

#if BOARD_LCD_SHOW_BOOT_PATTERN
    ESP_GOTO_ON_ERROR(board_lcd_draw_boot_pattern(), fail, TAG, "LCD boot pattern failed");
    vTaskDelay(pdMS_TO_TICKS(BOARD_LCD_BOOT_PATTERN_MS));
    ESP_GOTO_ON_ERROR(board_lcd_clear(0x0000), fail, TAG, "LCD clear after pattern failed");
#endif

    ESP_LOGI(TAG, "LCD initialized (%dx%d)", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;

fail:
    if (s_lcd_panel) {
        esp_lcd_panel_del(s_lcd_panel);
        s_lcd_panel = NULL;
    }
    if (s_lcd_io) {
        esp_lcd_panel_io_del(s_lcd_io);
        s_lcd_io = NULL;
    }
    if (spi_initialized_here) {
        (void)spi_bus_free(BOARD_LCD_SPI_HOST);
    }
    s_board_lcd_is_init = false;
    return ret;
}

static esp_err_t board_lcd_deinit(void)
{
    if (!s_board_lcd_is_init) {
        return ESP_OK;
    }

    gpio_set_level(BOARD_LCD_BACKLIGHT, 0);

    if (s_lcd_panel) {
        (void)esp_lcd_panel_del(s_lcd_panel);
        s_lcd_panel = NULL;
    }

    if (s_lcd_io) {
        (void)esp_lcd_panel_io_del(s_lcd_io);
        s_lcd_io = NULL;
    }

    esp_err_t ret = spi_bus_free(BOARD_LCD_SPI_HOST);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "LCD SPI bus free failed: %s", esp_err_to_name(ret));
    }

    s_board_lcd_is_init = false;
    return ESP_OK;
}

esp_err_t iot_board_init(void)
{
    if (s_board_is_init) {
        return ESP_OK;
    }

    usb_otg_route_to_internal_phy();

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << BOARD_IO_LED_GREEN) |
                        (1ULL << BOARD_IO_LED_YELLOW) |
                        (1ULL << BOARD_IO_HOST_BOOST_EN) |
                        (1ULL << BOARD_IO_DEV_VBUS_EN) |
                        (1ULL << BOARD_IO_USB_SEL) |
                        (1ULL << BOARD_IO_IDEV_LIMIT_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&out_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = create_gpio_button(BOARD_IO_BUTTON_OK, &s_btn_ok);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = create_gpio_button(BOARD_IO_BUTTON_UP, &s_btn_up);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = create_gpio_button(BOARD_IO_BUTTON_DW, &s_btn_dw);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = create_gpio_button(BOARD_IO_BUTTON_MENU, &s_btn_menu);
    if (ret != ESP_OK) {
        return ret;
    }

    iot_board_led_all_set_state(false);
    ESP_RETURN_ON_ERROR(iot_board_usb_set_mode(USB_DEVICE_MODE), TAG, "set USB mode failed");
    ESP_RETURN_ON_ERROR(iot_board_usb_device_set_power(true, false), TAG, "set USB power failed");
    ESP_RETURN_ON_ERROR(board_lcd_init(), TAG, "LCD init failed");

    s_board_is_init = true;
    ESP_LOGI(TAG, "Board init done (USB device mode/power on)");
    return ESP_OK;
}

esp_err_t iot_board_deinit(void)
{
    if (!s_board_is_init) {
        return ESP_OK;
    }

    (void)board_lcd_deinit();

    if (s_btn_ok) {
        iot_button_delete(s_btn_ok);
        s_btn_ok = NULL;
    }
    if (s_btn_up) {
        iot_button_delete(s_btn_up);
        s_btn_up = NULL;
    }
    if (s_btn_dw) {
        iot_button_delete(s_btn_dw);
        s_btn_dw = NULL;
    }
    if (s_btn_menu) {
        iot_button_delete(s_btn_menu);
        s_btn_menu = NULL;
    }

    s_board_is_init = false;
    return ESP_OK;
}

bool iot_board_is_init(void)
{
    return s_board_is_init;
}

board_res_handle_t iot_board_get_handle(board_res_id_t id)
{
    switch (id) {
        case BOARD_LCD_ID:
            return (board_res_handle_t)s_lcd_panel;
        case BOARD_BTN_OK_ID:
            return (board_res_handle_t)s_btn_ok;
        case BOARD_BTN_UP_ID:
            return (board_res_handle_t)s_btn_up;
        case BOARD_BTN_DW_ID:
            return (board_res_handle_t)s_btn_dw;
        case BOARD_BTN_MN_ID:
            return (board_res_handle_t)s_btn_menu;
        default:
            return NULL;
    }
}

char *iot_board_get_info(void)
{
    return "ESP32_S3_USB_OTG_EV(IDF6 compatibility BSP)";
}

esp_err_t iot_board_usb_device_set_power(bool on_off, bool from_battery)
{
    if (from_battery) {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, on_off);
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, !on_off);
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);
    } else {
        gpio_set_level(BOARD_IO_HOST_BOOST_EN, !on_off);
        gpio_set_level(BOARD_IO_DEV_VBUS_EN, on_off);
        gpio_set_level(BOARD_IO_IDEV_LIMIT_EN, on_off);
    }

    ESP_LOGI(TAG,
             "USB device power: %s (%s)",
             on_off ? "on" : "off",
             from_battery ? "battery" : "USB DEV");

    return ESP_OK;
}

bool iot_board_usb_device_get_power(void)
{
    return gpio_get_level(BOARD_IO_IDEV_LIMIT_EN) != 0;
}

float iot_board_get_host_voltage(void)
{
    return 0.0f;
}

float iot_board_get_battery_voltage(void)
{
    return 0.0f;
}

esp_err_t iot_board_usb_set_mode(usb_mode_t mode)
{
    if (mode == USB_DEVICE_MODE) {
        gpio_set_level(BOARD_IO_USB_SEL, 0);
        ESP_LOGI(TAG, "USB mode: device");
        return ESP_OK;
    }

    if (mode == USB_HOST_MODE) {
        gpio_set_level(BOARD_IO_USB_SEL, 1);
        ESP_LOGI(TAG, "USB mode: host");
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

usb_mode_t iot_board_usb_get_mode(void)
{
    return gpio_get_level(BOARD_IO_USB_SEL) ? USB_HOST_MODE : USB_DEVICE_MODE;
}

esp_err_t iot_board_button_register_cb(board_res_handle_t btn_handle, button_event_t event, button_cb_t cb)
{
    if (!btn_handle || !cb) {
        return ESP_ERR_INVALID_ARG;
    }

    return iot_button_register_cb((button_handle_t)btn_handle, event, NULL, cb, NULL);
}

esp_err_t iot_board_button_unregister_cb(board_res_handle_t btn_handle, button_event_t event)
{
    if (!btn_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return iot_button_unregister_cb((button_handle_t)btn_handle, event, NULL);
}

esp_err_t iot_board_led_set_state(int gpio_num, bool turn_on)
{
    if (gpio_num != BOARD_IO_LED_GREEN && gpio_num != BOARD_IO_LED_YELLOW) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio_set_level(gpio_num, turn_on ? 1 : 0);
    return ESP_OK;
}

esp_err_t iot_board_led_all_set_state(bool turn_on)
{
    for (size_t i = 0; i < sizeof(s_led_io) / sizeof(s_led_io[0]); i++) {
        gpio_set_level(s_led_io[i], turn_on ? 1 : 0);
    }
    return ESP_OK;
}

esp_err_t iot_board_led_toggle_state(int gpio_num)
{
    if (gpio_num != BOARD_IO_LED_GREEN && gpio_num != BOARD_IO_LED_YELLOW) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio_set_level(gpio_num, !gpio_get_level(gpio_num));
    return ESP_OK;
}

esp_err_t iot_board_wifi_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t board_lcd_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *img)
{
    if (!s_board_lcd_is_init || !s_lcd_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!img || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((uint32_t)x + width > BOARD_LCD_H_RES || (uint32_t)y + height > BOARD_LCD_V_RES) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_lcd_panel_draw_bitmap(s_lcd_panel, x, y, x + width, y + height, img);
}
