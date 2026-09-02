#include "ui.h"
#include "ssd1306.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void Error_Handler(void);

#define UI_HEADER_HEIGHT     11u
#define UI_TEXT_PAD_X         2u
#define UI_HEADER_TEXT_Y      2u
#define UI_RIGHT_EDGE_X     126u

#define UI_LIST_ROW0_Y       13u
#define UI_LIST_ROW_SPACING  10u
#define UI_LIST_ROWS          5u

#define UI_TEXT_LEN          24u

#define UI_CELL_SIZE         10u
#define UI_CELL_FIRST_X      14u
#define UI_CELL_PITCH_X      22u
#define UI_CELL_Y            43u
#define UI_CELL_INSET         2u
#define UI_CELL_FOCUS_PAD     2u

#define UI_TEMP_SCALE_MAX_C  50u

#define UI_LOG_LINES         12u
#define UI_LOG_VISIBLE       UI_LIST_ROWS
#define UI_LOG_TEXT_LEN      16u

typedef struct {
    uint32_t tick_ms;
    char     text[UI_LOG_TEXT_LEN];
} UI_LogEntry_t;

static const struct {
    const char *name;
    const char *pin_name;
} ui_outputs[OUT_COUNT] = {
    { OUT1_NAME, OUT1_PIN_NAME },
    { OUT2_NAME, OUT2_PIN_NAME },
    { OUT3_NAME, OUT3_PIN_NAME },
    { OUT4_NAME, OUT4_PIN_NAME },
    { OUT5_NAME, OUT5_PIN_NAME },
};

static ssd1306_t          ui_display;
static I2C_HandleTypeDef *ui_hi2c;
static uint8_t            ui_current_page = (uint8_t)UI_PAGE_HOME;
static uint8_t            ui_selected_output = 0u;
static bool               ui_bluetooth_connected = false;

static UI_LogEntry_t ui_log[UI_LOG_LINES];
static uint8_t       ui_log_count = 0u;
static uint8_t       ui_log_head = 0u;
static uint8_t       ui_log_scroll = 0u;

static void UI_DrawHeader(const char *title);
static void UI_DrawTextRight(uint16_t right_x, uint16_t y, const char *text, SSD1306_COLOR color);
static void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t percent);
static void UI_DrawCell(uint16_t x, uint16_t y, bool filled, bool focused);
static void UI_FormatUptime(char *out, size_t out_size, uint32_t now_ms);
static void UI_DrawHomePage(const UI_Data_t *data, uint32_t now_ms);
static void UI_DrawOutputsPage(const UI_Data_t *data);
static void UI_DrawSensorPage(const UI_Data_t *data);
static void UI_DrawLogPage(void);
static void UI_DrawHelpPage(void);

void UI_Init(I2C_HandleTypeDef *hi2c)
{
    ui_hi2c = hi2c;

    if (SSD1306_Init(hi2c) != HAL_OK) {
        Error_Handler();
    }

    ui_display.width = SSD1306_WIDTH;
    ui_display.height = SSD1306_HEIGHT;
    ui_current_page = (uint8_t)UI_PAGE_HOME;
    ui_selected_output = 0u;

    SSD1306_Clear(&ui_display);
    UI_DrawHeader("BOOTING");
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 18u, "STM32 BT NODE", SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 32u, "5 OUTPUTS  5 BUTTONS", SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 46u, "SYSTEM READY", SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(hi2c, &ui_display);
}

void UI_SetPage(uint8_t page)
{
    if (page < (uint8_t)UI_PAGE_COUNT) {
        ui_current_page = page;
    }
}

uint8_t UI_GetPage(void)
{
    return ui_current_page;
}

void UI_Log(const char *text)
{
    UI_LogEntry_t *entry;

    if (text == NULL) {
        return;
    }

    entry = &ui_log[ui_log_head];
    entry->tick_ms = HAL_GetTick();
    strncpy(entry->text, text, UI_LOG_TEXT_LEN - 1u);
    entry->text[UI_LOG_TEXT_LEN - 1u] = '\0';

    ui_log_head = (uint8_t)((ui_log_head + 1u) % UI_LOG_LINES);
    if (ui_log_count < UI_LOG_LINES) {
        ui_log_count++;
    }
}

void UI_Render(const UI_Data_t *data, uint32_t now_ms)
{
    if (data == NULL) {
        return;
    }

    SSD1306_Clear(&ui_display);
    ui_bluetooth_connected = data->bluetooth_connected;

    switch ((UI_Page_t)ui_current_page) {
    case UI_PAGE_HOME:
        UI_DrawHomePage(data, now_ms);
        break;
    case UI_PAGE_OUTPUTS:
        UI_DrawOutputsPage(data);
        break;
    case UI_PAGE_SENSOR:
        UI_DrawSensorPage(data);
        break;
    case UI_PAGE_LOG:
        UI_DrawLogPage();
        break;
    case UI_PAGE_HELP:
    case UI_PAGE_COUNT:
    default:
        UI_DrawHelpPage();
        break;
    }

    SSD1306_UpdateScreen(ui_hi2c, &ui_display);
}

static void UI_DrawHeader(const char *title)
{
    char right_text[12];

    SSD1306_FillRect(&ui_display, 0u, 0u, SSD1306_WIDTH, UI_HEADER_HEIGHT, SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_HEADER_TEXT_Y, title, SSD1306_COLOR_BLACK);

    snprintf(right_text, sizeof(right_text), "%s%u/%u",
             ui_bluetooth_connected ? "BT " : "",
             (unsigned)(ui_current_page + 1u), (unsigned)UI_PAGE_COUNT);

    UI_DrawTextRight(UI_RIGHT_EDGE_X, UI_HEADER_TEXT_Y, right_text, SSD1306_COLOR_BLACK);
}

static void UI_DrawTextRight(uint16_t right_x, uint16_t y, const char *text, SSD1306_COLOR color)
{
    uint16_t width = (uint16_t)(strlen(text) * SSD1306_CHAR_ADVANCE - 1u);

    if (width > right_x) {
        return;
    }

    SSD1306_WriteString(&ui_display, (uint16_t)(right_x - width), y, text, color);
}

static void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t percent)
{
    uint16_t inner_w;
    uint16_t fill_w;

    SSD1306_DrawRect(&ui_display, x, y, w, h, SSD1306_COLOR_WHITE);

    if (percent > 100u) {
        percent = 100u;
    }

    inner_w = (uint16_t)(w - 4u);
    fill_w = (uint16_t)((inner_w * percent) / 100u);

    if (fill_w > 0u) {
        SSD1306_FillRect(&ui_display, (uint16_t)(x + 2u), (uint16_t)(y + 2u),
                         fill_w, (uint16_t)(h - 4u), SSD1306_COLOR_WHITE);
    }
}

static void UI_DrawCell(uint16_t x, uint16_t y, bool filled, bool focused)
{
    SSD1306_DrawRect(&ui_display, x, y, UI_CELL_SIZE, UI_CELL_SIZE, SSD1306_COLOR_WHITE);

    if (filled) {
        SSD1306_FillRect(&ui_display, (uint16_t)(x + UI_CELL_INSET),
                         (uint16_t)(y + UI_CELL_INSET),
                         (uint16_t)(UI_CELL_SIZE - 2u * UI_CELL_INSET),
                         (uint16_t)(UI_CELL_SIZE - 2u * UI_CELL_INSET),
                         SSD1306_COLOR_WHITE);
    }

    if (focused) {
        SSD1306_DrawRect(&ui_display, (uint16_t)(x - UI_CELL_FOCUS_PAD),
                         (uint16_t)(y - UI_CELL_FOCUS_PAD),
                         (uint16_t)(UI_CELL_SIZE + 2u * UI_CELL_FOCUS_PAD),
                         (uint16_t)(UI_CELL_SIZE + 2u * UI_CELL_FOCUS_PAD),
                         SSD1306_COLOR_WHITE);
    }
}

static void UI_FormatUptime(char *out, size_t out_size, uint32_t now_ms)
{
    uint32_t seconds = now_ms / 1000u;

    snprintf(out, out_size, "%02u:%02u:%02u",
             (unsigned)((seconds / 3600u) % 100u),
             (unsigned)((seconds / 60u) % 60u),
             (unsigned)(seconds % 60u));
}

static void UI_DrawHomePage(const UI_Data_t *data, uint32_t now_ms)
{
    char     line[UI_TEXT_LEN];
    uint8_t  i;
    uint16_t temp_pct;

    UI_DrawHeader("HOME");

    snprintf(line, sizeof(line), "TEMP %u%cC", (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 12u, line, SSD1306_COLOR_WHITE);

    snprintf(line, sizeof(line), "HUMI %u%%", (unsigned)data->humidity_pct);
    SSD1306_WriteString(&ui_display, 70u, 12u, line, SSD1306_COLOR_WHITE);

    temp_pct = (uint16_t)((data->temperature_c * 100u) / UI_TEMP_SCALE_MAX_C);
    UI_DrawProgressBar(UI_TEXT_PAD_X, 21u, 56u, 8u, (uint8_t)((temp_pct > 100u) ? 100u : temp_pct));
    UI_DrawProgressBar(70u, 21u, 56u, 8u, data->humidity_pct);

    SSD1306_FillRect(&ui_display, 0u, 32u, SSD1306_WIDTH, 1u, SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 34u, "OUT", SSD1306_COLOR_WHITE);

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        uint16_t cell_x = (uint16_t)(UI_CELL_FIRST_X + i * UI_CELL_PITCH_X);
        char     number[2];

        number[0] = (char)('1' + i);
        number[1] = '\0';
        SSD1306_WriteString(&ui_display, (uint16_t)(cell_x + 2u), 34u, number, SSD1306_COLOR_WHITE);

        UI_DrawCell(cell_x, UI_CELL_Y, data->output_on[i], (i == ui_selected_output));
    }

    UI_FormatUptime(line, sizeof(line), now_ms);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 56u, line, SSD1306_COLOR_WHITE);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, 56u, data->sensor_valid ? "DHT OK" : "DHT --", SSD1306_COLOR_WHITE);
}

static void UI_DrawOutputsPage(const UI_Data_t *data)
{
    uint8_t i;

    UI_DrawHeader("OUTPUTS");

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        uint16_t      row_y = (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING);
        bool          selected = (i == ui_selected_output);
        SSD1306_COLOR text_color = selected ? SSD1306_COLOR_BLACK : SSD1306_COLOR_WHITE;
        char          line[UI_TEXT_LEN];

        if (selected) {
            SSD1306_FillRect(&ui_display, 0u, (uint16_t)(row_y - 1u), SSD1306_WIDTH, 9u, SSD1306_COLOR_WHITE);
        }

        snprintf(line, sizeof(line), "%u %-5s %-4s  %3s",
                 (unsigned)(i + 1u), ui_outputs[i].name, ui_outputs[i].pin_name,
                 data->output_on[i] ? "ON" : "OFF");
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, row_y, line, text_color);
    }
}

static void UI_DrawSensorPage(const UI_Data_t *data)
{
    char     line[UI_TEXT_LEN];
    uint16_t temp_pct;

    UI_DrawHeader("DHT11 SENSOR");

    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 13u, "TEMP", SSD1306_COLOR_WHITE);
    snprintf(line, sizeof(line), "%u%cC", (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, 13u, line, SSD1306_COLOR_WHITE);

    temp_pct = (uint16_t)((data->temperature_c * 100u) / UI_TEMP_SCALE_MAX_C);
    UI_DrawProgressBar(UI_TEXT_PAD_X, 22u, 124u, 8u, (uint8_t)((temp_pct > 100u) ? 100u : temp_pct));

    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 34u, "HUMI", SSD1306_COLOR_WHITE);
    snprintf(line, sizeof(line), "%u%%", (unsigned)data->humidity_pct);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, 34u, line, SSD1306_COLOR_WHITE);

    UI_DrawProgressBar(UI_TEXT_PAD_X, 43u, 124u, 8u, data->humidity_pct);

    if (data->sensor_valid) {
        snprintf(line, sizeof(line), "LAST OK %lus", (unsigned long)data->sensor_age_s);
    } else {
        snprintf(line, sizeof(line), "NO DATA");
    }
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 55u, line, SSD1306_COLOR_WHITE);

    UI_DrawTextRight(UI_RIGHT_EDGE_X, 55u,
                     data->bluetooth_connected ? "BT PAIR" : "BT ----",
                     SSD1306_COLOR_WHITE);
}

static void UI_DrawLogPage(void)
{
    char    header_range[12];
    uint8_t visible;
    uint8_t first;
    uint8_t i;

    UI_DrawHeader("LOG");

    if (ui_log_count == 0u) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_LIST_ROW0_Y, "(EMPTY)", SSD1306_COLOR_WHITE);
        return;
    }

    visible = (ui_log_count < UI_LOG_VISIBLE) ? ui_log_count : (uint8_t)UI_LOG_VISIBLE;
    first = (uint8_t)(ui_log_count - visible - ui_log_scroll);

    snprintf(header_range, sizeof(header_range), "%u-%u/%u",
             (unsigned)(first + 1u), (unsigned)(first + visible), (unsigned)ui_log_count);
    SSD1306_WriteString(&ui_display, 34u, UI_HEADER_TEXT_Y, header_range, SSD1306_COLOR_BLACK);

    for (i = 0u; i < visible; i++) {
        uint8_t index = (uint8_t)(first + i);
        uint8_t slot = (ui_log_count < UI_LOG_LINES)
                           ? index
                           : (uint8_t)((ui_log_head + index) % UI_LOG_LINES);
        const UI_LogEntry_t *entry = &ui_log[slot];
        uint32_t seconds = entry->tick_ms / 1000u;
        char     line[UI_TEXT_LEN];

        snprintf(line, sizeof(line), "%02u:%02u %s",
                 (unsigned)((seconds / 60u) % 100u), (unsigned)(seconds % 60u), entry->text);
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING),
                            line, SSD1306_COLOR_WHITE);
    }
}

static void UI_DrawHelpPage(void)
{
    static const char *const help_lines[UI_LIST_ROWS] = {
        "BTN: NEXT PREV UP DN",
        "     OK = SELECT",
        "SYSTEM READY",
        "CUSTOM REWRITE SKELETON",
        "",
    };
    uint8_t i;

    UI_DrawHeader("HELP");

    for (i = 0u; i < (uint8_t)UI_LIST_ROWS; i++) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING),
                            help_lines[i], SSD1306_COLOR_WHITE);
    }
}
