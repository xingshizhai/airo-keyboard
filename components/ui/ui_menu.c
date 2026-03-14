// 主菜单UI实现

#include "ui_menu.h"
#include "bsp_esp32_s3_usb_otg_ev.h"
#include "font_8x8.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "ui_menu";

// LCD显示参数
#define UI_LCD_W 240
#define UI_LCD_H 240

// 菜单项高度
#define MENU_ITEM_H 50  // 固定高度，避免整数除法问题
#define MENU_ITEM_PADDING 10
#define MENU_TITLE_H 15  // 标题栏高度
#define MENU_LABEL_SCALE 2
#define MENU_LABEL_LEFT_PADDING 15

// 颜色定义
#define UI_COLOR_BG 0x0841
#define UI_COLOR_ITEM_BG 0x2104
#define UI_COLOR_ITEM_SELECTED 0xFD20
#define UI_COLOR_TEXT 0xFFFF
#define UI_COLOR_TEXT_SELECTED 0x0000
#define UI_COLOR_BORDER 0x7BEF

static const char *s_menu_labels[UI_MENU_ITEM_COUNT] = {
    "TAB+ALT",
    "WIN+Q",
    "KEYBOARD",
    "Wi-Fi INFO",
};

// 当前选中项
static ui_menu_item_t current_item = UI_MENU_ITEM_COMBO_TAB_ALT;

// WiFi状态
static bool s_wifi_connected = false;
static char s_wifi_ip_address[16] = "";

// 行缓冲区
static uint16_t s_line_buf[UI_LCD_W];

// 静态函数前向声明
static esp_err_t draw_filled_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
static esp_err_t draw_char_from_font(uint16_t x, uint16_t y, char ch, uint16_t scale, uint16_t color);
static esp_err_t draw_menu_label(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t scale);
static esp_err_t draw_menu_item(ui_menu_item_t item, bool selected);

/**
 * @brief 绘制填充矩形
 */
static esp_err_t draw_filled_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (width == 0 || height == 0) {
        return ESP_OK;
    }

    if ((uint32_t)x + width > UI_LCD_W || (uint32_t)y + height > UI_LCD_H) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint16_t i = 0; i < width; i++) {
        s_line_buf[i] = color;
    }

    for (uint16_t row = 0; row < height; row++) {
        esp_err_t ret = board_lcd_draw_image(x, y + row, width, 1, s_line_buf);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

/**
 * @brief 从字体库中绘制字符
 */
static esp_err_t draw_char_from_font(uint16_t x, uint16_t y, char ch, uint16_t scale, uint16_t color)
{
    // 从字体库中获取字符并放大绘制
    if (ch < 32 || ch > 126) {
        return ESP_OK;  // 超出字体范围
    }
    
    const font_8x8_char_t *font_char = &font_8x8_table[ch - 32];
    
    // 对每行点阵进行放大绘制
    for (uint8_t py = 0; py < 8; py++) {
        uint8_t row_bits = font_char->data[py];
        
        for (uint8_t px = 0; px < 8; px++) {
            if (row_bits & (0x80 >> px)) {
                // 该像素点被置位，绘制放大后的方块
                uint16_t px_x = x + px * scale;
                uint16_t px_y = y + py * scale;
                
                if (px_x + scale <= UI_LCD_W && px_y + scale <= UI_LCD_H) {
                    ESP_RETURN_ON_ERROR(draw_filled_rect(px_x, px_y, scale, scale, color), TAG, "char_pixel failed");
                }
            }
        }
    }
    
    return ESP_OK;
}

/**
 * @brief 绘制简化的菜单项标签（使用字体库）
 */
static esp_err_t draw_menu_label(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t scale)
{
    uint16_t curr_x = x;
    
    for (size_t i = 0; text[i] != '\0' && curr_x < UI_LCD_W; i++) {
        esp_err_t err = draw_char_from_font(curr_x, y, text[i], scale, color);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw char '%c' (0x%02x): %s", text[i], text[i], esp_err_to_name(err));
            // 继续绘制下一个字符，不返回错误
        }
        curr_x += 8 * scale + 1;  // 字符宽度 + 间距
    }
    
    return ESP_OK;
}

/**
 * @brief 绘制菜单项
 */
static esp_err_t draw_menu_item(ui_menu_item_t item, bool selected)
{
    // 菜单项从标题栏下方开始
    uint16_t y = MENU_TITLE_H + MENU_ITEM_PADDING + item * MENU_ITEM_H;
    uint16_t item_h = MENU_ITEM_H - 2 * MENU_ITEM_PADDING;
    
    // 背景色
    uint16_t bg_color = selected ? UI_COLOR_ITEM_SELECTED : UI_COLOR_ITEM_BG;
    uint16_t text_color = selected ? UI_COLOR_TEXT_SELECTED : UI_COLOR_TEXT;
    
    // 绘制菜单项背景
    ESP_RETURN_ON_ERROR(
        draw_filled_rect(MENU_ITEM_PADDING, y, UI_LCD_W - 2 * MENU_ITEM_PADDING, item_h, bg_color),
        TAG, "item bg failed");
    
    // 绘制选中状态边框（简化为1像素）
    if (selected) {
        // 上边框
        ESP_RETURN_ON_ERROR(
            draw_filled_rect(MENU_ITEM_PADDING, y, UI_LCD_W - 2 * MENU_ITEM_PADDING, 1, UI_COLOR_BORDER),
            TAG, "border top failed");
        // 下边框
        ESP_RETURN_ON_ERROR(
            draw_filled_rect(MENU_ITEM_PADDING, y + item_h - 1, UI_LCD_W - 2 * MENU_ITEM_PADDING, 1, UI_COLOR_BORDER),
            TAG, "border bottom failed");
        // 左边框
        ESP_RETURN_ON_ERROR(
            draw_filled_rect(MENU_ITEM_PADDING, y, 1, item_h, UI_COLOR_BORDER),
            TAG, "border left failed");
        // 右边框
        ESP_RETURN_ON_ERROR(
            draw_filled_rect(UI_LCD_W - MENU_ITEM_PADDING - 1, y, 1, item_h, UI_COLOR_BORDER),
            TAG, "border right failed");
    }
    
    // 计算标签位置（垂直居中）
    uint16_t label_h = 8 * MENU_LABEL_SCALE;
    uint16_t label_y = y + (item_h - label_h) / 2;
    uint16_t label_x = MENU_ITEM_PADDING + MENU_LABEL_LEFT_PADDING;
    
    // 绘制标签
    if (item < UI_MENU_ITEM_COUNT) {
        ESP_RETURN_ON_ERROR(
            draw_menu_label(label_x, label_y, s_menu_labels[item], text_color, MENU_LABEL_SCALE),
            TAG, "menu label failed");
    }
    
    return ESP_OK;
}

/**
 * @brief 初始化主菜单UI
 */
esp_err_t ui_menu_init(void)
{
    ESP_LOGI(TAG, "Initializing main menu UI, item count: %d", UI_MENU_ITEM_COUNT);
    ESP_LOGI(TAG, "Menu item height: %d, total height: %d", MENU_ITEM_H, 
             MENU_TITLE_H + 2 * MENU_ITEM_PADDING + UI_MENU_ITEM_COUNT * MENU_ITEM_H);
    
    current_item = UI_MENU_ITEM_COMBO_TAB_ALT;
    
    return ui_menu_refresh_display();
}

/**
 * @brief 反初始化主菜单UI
 */
esp_err_t ui_menu_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing main menu UI");
    return ESP_OK;
}

/**
 * @brief 获取当前选中的菜单项
 */
ui_menu_item_t ui_menu_get_selected_item(void)
{
    return current_item;
}

/**
 * @brief 处理导航事件
 */
esp_err_t ui_menu_handle_event(ui_menu_nav_event_t nav_event)
{
    ui_menu_item_t prev_item = current_item;
    
    switch (nav_event) {
        case UI_MENU_NAV_UP:
            // 上按键：向下移动到下一个菜单项（按键方向修正）
            if (current_item < UI_MENU_ITEM_COUNT - 1) {
                current_item++;
            } else {
                current_item = 0;  // 循环到第一项
            }
            break;
            
        case UI_MENU_NAV_DOWN:
            // 下按键：向上移动到上一个菜单项（按键方向修正）
            if (current_item > 0) {
                current_item--;
            } else {
                current_item = UI_MENU_ITEM_COUNT - 1;  // 循环到最后一项
            }
            break;
            
        case UI_MENU_NAV_SELECT:
            ESP_LOGI(TAG, "Selected menu item: %d", current_item);
            return ESP_OK;  // 不刷新，由外部处理选择
            
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    // 增量刷新：只重绘改变的菜单项
    ESP_RETURN_ON_ERROR(draw_menu_item(prev_item, false), TAG, "redraw prev failed");
    ESP_RETURN_ON_ERROR(draw_menu_item(current_item, true), TAG, "redraw curr failed");
    
    ESP_LOGD(TAG, "Menu item changed: %d -> %d", prev_item, current_item);
    
    return ESP_OK;
}

/**
 * @brief 刷新菜单显示
 */
esp_err_t ui_menu_refresh_display(void)
{
    ESP_LOGI(TAG, "Starting menu display refresh...");
    
    // 清空背景
    ESP_RETURN_ON_ERROR(draw_filled_rect(0, 0, UI_LCD_W, UI_LCD_H, UI_COLOR_BG), TAG, "clear bg failed");
    
    // 绘制标题栏（顶部彩色条）
    ESP_RETURN_ON_ERROR(
        draw_filled_rect(0, 0, UI_LCD_W, MENU_TITLE_H, UI_COLOR_ITEM_SELECTED),
        TAG, "title bar failed");
    
    // 绘制所有菜单项
    for (ui_menu_item_t item = 0; item < UI_MENU_ITEM_COUNT; item++) {
        bool selected = (item == current_item);
        esp_err_t err = draw_menu_item(item, selected);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw menu item %d: %s", item, esp_err_to_name(err));
            // 继续绘制其他项
        } else {
            ESP_LOGI(TAG, "Menu item %d drawn successfully (selected: %d)", item, selected);
        }
    }
    
    ESP_LOGI(TAG, "Menu display refreshed, selected: %d", current_item);
    
    return ESP_OK;
}

/**
 * @brief 设置WiFi连接状态
 */
void ui_menu_set_wifi_status(bool connected, const char *ip_address)
{
    s_wifi_connected = connected;
    if (ip_address != NULL) {
        strncpy(s_wifi_ip_address, ip_address, sizeof(s_wifi_ip_address) - 1);
        s_wifi_ip_address[sizeof(s_wifi_ip_address) - 1] = '\0';
    } else {
        s_wifi_ip_address[0] = '\0';
    }
    ESP_LOGI(TAG, "WiFi status updated: connected=%d, ip=%s", connected, s_wifi_ip_address);
}

/**
 * @brief 显示Wi-Fi信息页面
 */
esp_err_t ui_menu_show_wifi_info(void)
{
    // 清空背景
    ESP_RETURN_ON_ERROR(draw_filled_rect(0, 0, UI_LCD_W, UI_LCD_H, UI_COLOR_BG), TAG, "clear bg failed");
    
    // 绘制标题栏
    ESP_RETURN_ON_ERROR(
        draw_filled_rect(0, 0, UI_LCD_W, MENU_TITLE_H, UI_COLOR_ITEM_SELECTED),
        TAG, "title bar failed");
    
    // 绘制标题文本（放大3倍）
    uint16_t title_y = MENU_TITLE_H + 15;
    uint16_t title_x = 20;
    ESP_RETURN_ON_ERROR(
        draw_menu_label(title_x, title_y, "Wi-Fi Info", UI_COLOR_TEXT, 3),
        TAG, "title label failed");
    
    // 绘制状态信息（放大2倍）
    uint16_t line_y = title_y + 60;  // 增加间距适应大字体
    uint16_t line_x = 30;
    uint16_t line_spacing = 45;  // 增加行间距
    
    // 连接状态
    char status_str[32];
    if (s_wifi_connected) {
        snprintf(status_str, sizeof(status_str), "Connected");
    } else {
        snprintf(status_str, sizeof(status_str), "Disconnected");
    }
    ESP_RETURN_ON_ERROR(
        draw_menu_label(line_x, line_y, status_str, UI_COLOR_TEXT, 2),
        TAG, "status label failed");
    
    // IP地址
    line_y += line_spacing;
    char ip_str[32];
    if (s_wifi_ip_address[0] != '\0') {
        snprintf(ip_str, sizeof(ip_str), "IP: %s", s_wifi_ip_address);
    } else {
        snprintf(ip_str, sizeof(ip_str), "IP: N/A");
    }
    ESP_RETURN_ON_ERROR(
        draw_menu_label(line_x, line_y, ip_str, UI_COLOR_TEXT, 2),
        TAG, "ip label failed");
    
    ESP_LOGI(TAG, "Wi-Fi info displayed: connected=%d, ip=%s", s_wifi_connected, s_wifi_ip_address);
    
    return ESP_OK;
}
