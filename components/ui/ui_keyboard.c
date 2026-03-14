// 虚拟键盘UI实现 - 分页大字符布局

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "bsp_esp32_s3_usb_otg_ev.h"
#include "ui_keyboard.h"
#include "font_8x8.h"

static const char *TAG = "ui_keyboard";
#define UI_KEY_ENTER_TOKEN ((char)0x1F)

// 分页键盘布局 - 5列 x 4行 = 20字符/页
// 页1: 数字 + 部分小写
static const char *keyboard_page_1[] = {
    "01234",
    "56789",
    "abcde",
    "fghij"
};

// 页2: 其余小写 + 部分大写
static const char *keyboard_page_2[] = {
    "klmno",
    "pqrst",
    "uvwxy",
    "zABCD"
};

// 页3: 大写字母
static const char *keyboard_page_3[] = {
    "EFGHI",
    "JKLMN",
    "OPQRS",
    "TUVWX"
};

// 页4: 大写尾部 + 常用符号（包含 Enter 键）
static const char keyboard_page_4_row_4[] = {
    '-',
    '=',
    UI_KEY_ENTER_TOKEN,
    '\\',
    '|',
    '\0',
};

static const char *keyboard_page_4[] = {
    "YZ.,;",
    "!?@#$",
    "%^&*+",
    keyboard_page_4_row_4
};

// 页面列表
static const char **keyboard_pages[] = {
    keyboard_page_1,
    keyboard_page_2,
    keyboard_page_3,
    keyboard_page_4,
};

#define UI_LCD_W 240
#define UI_LCD_H 240
#define KEYBOARD_COLS 5
#define KEYBOARD_ROWS 4
#define KEYBOARD_PAGES (sizeof(keyboard_pages) / sizeof(keyboard_pages[0]))

#define UI_INDICATOR_H 12
#define UI_GRID_H (UI_LCD_H - UI_INDICATOR_H)

#define UI_CELL_W (UI_LCD_W / KEYBOARD_COLS)
#define UI_CELL_H (UI_GRID_H / KEYBOARD_ROWS)

#define UI_COLOR_BG 0x0841
#define UI_COLOR_EMPTY 0x18C3
#define UI_COLOR_KEY 0x39C7
#define UI_COLOR_KEY_BORDER 0x7BEF
#define UI_COLOR_KEY_SELECTED 0xFD20
#define UI_COLOR_KEY_SELECTED_BORDER 0xFFFF
#define UI_COLOR_MARKER 0xC618
#define UI_COLOR_MARKER_SELECTED 0x0000
#define UI_COLOR_PAGE_BG 0x2945

// 键盘状态
static uint8_t current_page = 0;
static uint8_t current_row = 0;
static uint8_t current_col = 0;
static uint8_t prev_row = 0;
static uint8_t prev_col = 0;

static uint16_t s_line_buf[UI_LCD_W];

static bool is_selectable_key(uint8_t page, uint8_t row, uint8_t col)
{
    if (page >= KEYBOARD_PAGES || row >= KEYBOARD_ROWS || col >= KEYBOARD_COLS) {
        return false;
    }

    const char *line = keyboard_pages[page][row];
    if (!line) {
        return false;
    }

    size_t len = strlen(line);
    if (col >= len) {
        return false;
    }

    return (line[col] != '\0' && line[col] != ' ');
}

static void step_selection(bool forward)
{
    uint8_t max_steps = KEYBOARD_ROWS * KEYBOARD_COLS;

    for (uint8_t i = 0; i < max_steps; i++) {
        if (forward) {
            current_col++;
            if (current_col >= KEYBOARD_COLS) {
                current_col = 0;
                current_row = (current_row + 1) % KEYBOARD_ROWS;
            }
        } else {
            if (current_col == 0) {
                current_col = KEYBOARD_COLS - 1;
                current_row = (current_row == 0) ? (KEYBOARD_ROWS - 1) : (current_row - 1);
            } else {
                current_col--;
            }
        }

        if (is_selectable_key(current_page, current_row, current_col)) {
            return;
        }
    }
}

static void ensure_selection_valid(void)
{
    if (is_selectable_key(current_page, current_row, current_col)) {
        return;
    }

    current_row = 0;
    current_col = 0;
    if (is_selectable_key(current_page, current_row, current_col)) {
        return;
    }

    step_selection(true);
}

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

static esp_err_t draw_key_cell(uint8_t row, uint8_t col, char ch, bool selected)
{
    uint16_t x = col * UI_CELL_W;
    uint16_t y = row * UI_CELL_H;
    uint16_t w = UI_CELL_W;
    uint16_t h = UI_CELL_H;

    uint16_t border = selected ? UI_COLOR_KEY_SELECTED_BORDER : UI_COLOR_KEY_BORDER;
    uint16_t fill = selected ? UI_COLOR_KEY_SELECTED : UI_COLOR_KEY;

    ESP_RETURN_ON_ERROR(draw_filled_rect(x, y, w, h, border), TAG, "border failed");
    if (w > 4 && h > 4) {
        ESP_RETURN_ON_ERROR(draw_filled_rect(x + 2, y + 2, w - 4, h - 4, fill), TAG, "fill failed");
    }

    // 在单元格中绘制真实字符（来自字体库）
    if (ch != '\0' && ch != ' ') {
        // Enter 使用专用 token，在屏幕上显示为 'E'
        char render_ch = (ch == UI_KEY_ENTER_TOKEN) ? 'E' : ch;

        // 根据单元格宽高计算缩放，确保字符不会越界
        uint16_t avail_w = (w > 12) ? (w - 12) : w;
        uint16_t avail_h = (h > 12) ? (h - 12) : h;
        uint16_t scale_w = avail_w / 8;
        uint16_t scale_h = avail_h / 8;
        uint16_t scale = (scale_w < scale_h) ? scale_w : scale_h;
        if (scale < 1) {
            scale = 1;
        }

        uint16_t char_width = 8 * scale;
        uint16_t char_height = 8 * scale;

        // 字符在单元格内居中
        uint16_t char_x = x + (w - char_width) / 2;
        uint16_t char_y = y + (h - char_height) / 2;

        uint16_t char_color = selected ? UI_COLOR_MARKER_SELECTED : UI_COLOR_MARKER;
        ESP_RETURN_ON_ERROR(draw_char_from_font(char_x, char_y, render_ch, scale, char_color), TAG, "char_render failed");
    }

    return ESP_OK;
}

static esp_err_t draw_empty_cell(uint8_t row, uint8_t col)
{
    uint16_t x = col * UI_CELL_W;
    uint16_t y = row * UI_CELL_H;

    ESP_RETURN_ON_ERROR(draw_filled_rect(x, y, UI_CELL_W, UI_CELL_H, UI_COLOR_KEY_BORDER), TAG, "border failed");
    if (UI_CELL_W > 4 && UI_CELL_H > 4) {
        ESP_RETURN_ON_ERROR(draw_filled_rect(x + 2, y + 2, UI_CELL_W - 4, UI_CELL_H - 4, UI_COLOR_EMPTY), TAG, "fill failed");
    }

    return ESP_OK;
}

static esp_err_t draw_page_indicator(void)
{
    uint16_t bar_width = (UI_LCD_W * (current_page + 1)) / KEYBOARD_PAGES;

    // 底部页面进度条
    ESP_RETURN_ON_ERROR(draw_filled_rect(0, UI_LCD_H - UI_INDICATOR_H, UI_LCD_W, UI_INDICATOR_H, 0x2965), TAG, "indicator bg failed");
    ESP_RETURN_ON_ERROR(draw_filled_rect(0, UI_LCD_H - UI_INDICATOR_H, bar_width, UI_INDICATOR_H, 0xFD20), TAG, "indicator bar failed");

    return ESP_OK;
}

/**
 * @brief 初始化UI键盘
 */
esp_err_t ui_keyboard_init(void)
{
    ESP_LOGI(TAG, "Initializing UI keyboard (paged layout: 5x4, %d pages)", KEYBOARD_PAGES);
    
    current_page = 0;
    current_row = 0;
    current_col = 0;
    prev_row = 0;
    prev_col = 0;
    ensure_selection_valid();

    return ui_keyboard_refresh_display();
}

/**
 * @brief 反初始化UI键盘
 */
esp_err_t ui_keyboard_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing UI keyboard");
    return ESP_OK;
}

/**
 * @brief 获取当前选中的字符
 */
char ui_keyboard_get_selected_char(void)
{
    if (!is_selectable_key(current_page, current_row, current_col)) {
        return '\0';
    }

    return keyboard_pages[current_page][current_row][current_col];
}

/**
 * @brief 增量刷新显示（仅更新改变的单元格）
 */
static esp_err_t ui_keyboard_update_selection(void)
{
    // 重绘之前选中的单元格（取消选中状态）
    if (prev_row < KEYBOARD_ROWS && prev_col < KEYBOARD_COLS) {
        const char *line = keyboard_pages[current_page][prev_row];
        size_t line_len = line ? strlen(line) : 0;
        if (prev_col < line_len) {
            ESP_RETURN_ON_ERROR(draw_key_cell(prev_row, prev_col, line[prev_col], false), TAG, "prev cell failed");
        } else {
            ESP_RETURN_ON_ERROR(draw_empty_cell(prev_row, prev_col), TAG, "prev empty failed");
        }
    }

    // 重绘当前选中的单元格（选中状态）
    if (current_row < KEYBOARD_ROWS && current_col < KEYBOARD_COLS) {
        const char *line = keyboard_pages[current_page][current_row];
        size_t line_len = line ? strlen(line) : 0;
        if (current_col < line_len) {
            ESP_RETURN_ON_ERROR(draw_key_cell(current_row, current_col, line[current_col], true), TAG, "curr cell failed");
        } else {
            ESP_RETURN_ON_ERROR(draw_empty_cell(current_row, current_col), TAG, "curr empty failed");
        }
    }

    return ESP_OK;
}

/**
 * @brief 处理按键事件
 */
esp_err_t ui_keyboard_handle_event(ui_keyboard_nav_event_t nav_event)
{
    switch (nav_event) {
        case UI_KEYBOARD_NAV_UP:
        case UI_KEYBOARD_NAV_DOWN:
        case UI_KEYBOARD_NAV_LEFT:
        case UI_KEYBOARD_NAV_RIGHT:
            // 保存当前位置作为上一次位置
            prev_row = current_row;
            prev_col = current_col;
            
            // 移动选择
            if (nav_event == UI_KEYBOARD_NAV_UP) {
                step_selection(true);
            } else if (nav_event == UI_KEYBOARD_NAV_DOWN) {
                step_selection(false);
            } else if (nav_event == UI_KEYBOARD_NAV_LEFT) {
                step_selection(false);
            } else {
                step_selection(true);
            }
            
            // 增量刷新（仅更新改变的单元格）
            return ui_keyboard_update_selection();

        case UI_KEYBOARD_NAV_SELECT: {
            char selected = ui_keyboard_get_selected_char();
            if (selected == UI_KEY_ENTER_TOKEN) {
                ESP_LOGI(TAG, "Selected: ENTER (page %d)", current_page + 1);
            } else {
                ESP_LOGI(TAG, "Selected: '%c' (page %d)", selected, current_page + 1);
            }
            break;
        }

        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief 切换到下一页
 */
void ui_keyboard_next_page(void)
{
    if (current_page < KEYBOARD_PAGES - 1) {
        current_page++;
    } else {
        current_page = 0;  // 循环到第一页
    }
    prev_row = current_row;
    prev_col = current_col;
    current_row = 0;
    current_col = 0;
    ensure_selection_valid();
    ESP_LOGI(TAG, "Page switched to %d/%d", current_page + 1, KEYBOARD_PAGES);
}

/**
 * @brief 切换到上一页
 */
void ui_keyboard_prev_page(void)
{
    if (current_page > 0) {
        current_page--;
    } else {
        current_page = KEYBOARD_PAGES - 1;  // 循环到最后一页
    }
    prev_row = current_row;
    prev_col = current_col;
    current_row = 0;
    current_col = 0;
    ensure_selection_valid();
    ESP_LOGI(TAG, "Page switched to %d/%d", current_page + 1, KEYBOARD_PAGES);
}

/**
 * @brief 刷新键盘显示
 */
esp_err_t ui_keyboard_refresh_display(void)
{
    ESP_RETURN_ON_ERROR(draw_filled_rect(0, 0, UI_LCD_W, UI_GRID_H, UI_COLOR_BG), TAG, "background failed");

    if (current_page < KEYBOARD_PAGES) {
        for (uint8_t row = 0; row < KEYBOARD_ROWS; row++) {
            const char *line = keyboard_pages[current_page][row];
            size_t line_len = line ? strlen(line) : 0;

            for (uint8_t col = 0; col < KEYBOARD_COLS; col++) {
                if (col < line_len) {
                    bool selected = (row == current_row) && (col == current_col);
                    ESP_RETURN_ON_ERROR(draw_key_cell(row, col, line[col], selected), TAG, "key cell failed");
                } else {
                    ESP_RETURN_ON_ERROR(draw_empty_cell(row, col), TAG, "empty cell failed");
                }
            }
        }
    }

    ESP_RETURN_ON_ERROR(draw_page_indicator(), TAG, "page indicator failed");

    ESP_LOGD(TAG, "Page %d/%d, Row %d, Col %d, Char: '%c'",
             current_page + 1, KEYBOARD_PAGES,
             current_row, current_col,
             ui_keyboard_get_selected_char());

    return ESP_OK;
}
