# Clean Hardware & Interrupt Scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create branch `refactor/clean-hardware-scaffold`, remove unused custom libraries (`uart`, `Digital_Out`, `Ring_Buffer`, `Command_Selector`), strip state machine logic from the UI module while preserving screen drawing functions, and reduce `main.c` to peripheral initialization and interrupt callback stubs.

**Architecture:** A minimalist firmware boilerplate for STM32F103 (Blue Pill) that preserves all clock, GPIO, USART2, I2C2, TIM2, NVIC, and MSP hardware setup along with drivers for `SSD1306` and `DHT11`, providing a clean foundation to rewrite system logic from scratch.

**Tech Stack:** C11, STM32 HAL Driver, CMSIS Cortex-M3, CMake, Ninja, GCC ARM Embedded Toolchain (`arm-none-eabi-gcc`).

**Spec:** In-chat bounded design approved by user on 2026-09-02.

## Global Constraints

- **Target MCU:** STM32F103C8T6 (Blue Pill), 72 MHz SYSCLK, 36 MHz APB1, 72 MHz APB2.
- **Naming Conventions:**
  - File names: standard lowercase (`main.c`, `ui.c`, `ui.h`, `pin_config.h`, `dht11.c`, `ssd1306.c`).
  - Public functions: `Module_ActionName()` (e.g., `DHT11_Init()`, `SSD1306_Init()`, `UI_Init()`).
  - Types/Structs: `PascalCase_t` (e.g., `DHT11_Config_t`, `UI_Data_t`).
  - Macros & Constants: `UPPER_SNAKE_CASE` (e.g., `DHT11_TIMEOUT_US`, `OUT1_PIN`).
  - Internal variables & handles: `snake_case` (e.g., `huart2`, `hi2c2`, `htim2`).
- **Toolchain & Build:** Must compile with 0 errors and 0 warnings using CMake and Ninja.

---

### Task 1: Create Branch and Remove Unused Libraries

**Files:**
- Delete: `stm-firmware/lib/Src/Digital_Out.c`
- Delete: `stm-firmware/lib/Inc/Digital_Out.h`
- Delete: `stm-firmware/lib/Src/uart.c`
- Delete: `stm-firmware/lib/Inc/uart.h`
- Delete: `stm-firmware/lib/Src/Command_Selector.c`
- Delete: `stm-firmware/lib/Inc/Command_Selector.h`
- Delete: `stm-firmware/lib/Src/Ring_Buffer.c`
- Delete: `stm-firmware/lib/Inc/Ring_Buffer.h`

**Interfaces:**
- Consumes: Git repository
- Produces: Cleaned `lib/` directory containing only `DHT11`, `SSD1306`, `font5x7`, `pin_config.h`, `main.h`, `Global_Enum.h`.

- [ ] **Step 1: Create and switch to new branch**

```bash
git checkout -b refactor/clean-hardware-scaffold
```

- [ ] **Step 2: Delete unused library files**

Delete the following files:
- `stm-firmware/lib/Src/Digital_Out.c`
- `stm-firmware/lib/Inc/Digital_Out.h`
- `stm-firmware/lib/Src/uart.c`
- `stm-firmware/lib/Inc/uart.h`
- `stm-firmware/lib/Src/Command_Selector.c`
- `stm-firmware/lib/Inc/Command_Selector.h`
- `stm-firmware/lib/Src/Ring_Buffer.c`
- `stm-firmware/lib/Inc/Ring_Buffer.h`

- [ ] **Step 3: Commit removal of unused libraries**

```bash
git add stm-firmware/lib/
git commit -m "refactor(lib): remove uart, digital_out, ring_buffer and command_selector libraries"
```

---

### Task 2: Strip Logic and State Machines from UI Module

**Files:**
- Modify: `stm-firmware/src/ui.h`
- Modify: `stm-firmware/src/ui.c`

**Interfaces:**
- Consumes: `SSD1306.h`, `pin_config.h`, `main.h`
- Produces:
  - `void UI_Init(I2C_HandleTypeDef *hi2c);`
  - `void UI_Render(const UI_Data_t *data, uint32_t now_ms);`
  - `void UI_SetPage(uint8_t page);`
  - `uint8_t UI_GetPage(void);`
  - `void UI_Log(const char *text);`

- [ ] **Step 1: Update `stm-firmware/src/ui.h`**

Update `ui.h` to retain UI data representations and rendering function declarations without button debouncing / event task APIs:

```c
#ifndef UI_H
#define UI_H

#include "main.h"
#include <stdbool.h>

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_OUTPUTS,
    UI_PAGE_SENSOR,
    UI_PAGE_LOG,
    UI_PAGE_HELP,
    UI_PAGE_COUNT
} UI_Page_t;

typedef struct {
    uint8_t  temperature_c;
    uint8_t  humidity_pct;
    bool     output_on[OUT_COUNT];
    bool     heartbeat_led_on;
    bool     bluetooth_connected;
    bool     sensor_valid;
    uint32_t sensor_age_s;
} UI_Data_t;

void UI_Init(I2C_HandleTypeDef *hi2c);
void UI_Render(const UI_Data_t *data, uint32_t now_ms);
void UI_SetPage(uint8_t page);
uint8_t UI_GetPage(void);
void UI_Log(const char *text);

#endif /* UI_H */
```

- [ ] **Step 2: Update `stm-firmware/src/ui.c`**

Replace `ui.c` content with pure display rendering functions, eliminating `UI_Task`, `UI_SampleButton`, `UI_ReleaseStaleButtons`, `UI_HandleButtonIrq`, and button event queues:

```c
#include "ui.h"
#include "SSD1306.h"

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
```

- [ ] **Step 3: Commit UI changes**

```bash
git add stm-firmware/src/ui.h stm-firmware/src/ui.c
git commit -m "refactor(ui): strip button debouncing FSM and keep direct display renderers"
```

---

### Task 3: Clean `main.c` Down to Hardware Setup & Interrupt Callbacks

**Files:**
- Modify: `stm-firmware/src/main.c`

**Interfaces:**
- Consumes: `main.h`, `DHT11.h`, `SSD1306.h`, `ui.h`, STM32 HAL
- Produces:
  - Peripheral handles: `UART_HandleTypeDef huart2;`, `I2C_HandleTypeDef hi2c2;`, `TIM_HandleTypeDef htim2;`
  - Functions: `SystemClock_Config(void);`, `Error_Handler(void);`
  - HAL Callbacks: `HAL_UART_RxCpltCallback()`, `HAL_UART_ErrorCallback()`, `HAL_GPIO_EXTI_Callback()`, `HAL_TIM_PeriodElapsedCallback()`
  - Entry point: `int main(void)`

- [ ] **Step 1: Rewrite `stm-firmware/src/main.c`**

Replace `main.c` with hardware initialization routines and callback stubs:

```c
/*
 * main.c — Hardware setup & interrupt skeleton for STM32F103 (Blue Pill).
 */
#include "main.h"

#include "DHT11.h"
#include "SSD1306.h"
#include "ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*==================== Peripheral Handles ====================*/

UART_HandleTypeDef huart2;
I2C_HandleTypeDef hi2c2;
TIM_HandleTypeDef htim2;

/*==================== Function Prototypes ====================*/

void SystemClock_Config(void);
void Error_Handler(void);

static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM2_Init(void);

#define TIM2_COUNTER_FREQ_HZ 1000000u

/*==================== Main Entry Point ====================*/

int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock to 72 MHz */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C2_Init();
    MX_TIM2_Init();

    /* Initialize UI / Display */
    UI_Init(&hi2c2);

    /* Main superloop */
    while (1) {
        /* User logic goes here */
    }
}

/*==================== System Clock Configuration ====================*/

/*
 * SYSCLK = 72 MHz: HSE 8 MHz -> PLL x9.
 *   HCLK  = 72 MHz (AHB  /1)
 *   PCLK2 = 72 MHz (APB2 /1)
 *   PCLK1 = 36 MHz (APB1 /2)
 *   TIM2  = 72 MHz (APB1 prescaler != 1 -> timer clock = 2 x PCLK1)
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init.HSEState = RCC_HSE_ON;
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        Error_Handler();
    }

    rcc_clk_init.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/*==================== Peripheral Initializations ====================*/

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Heartbeat LED (PC13, active LOW) */
    HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, HEARTBEAT_LED_OFF_STATE);
    gpio_init.Pin = HEARTBEAT_LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HEARTBEAT_LED_PORT, &gpio_init);

    /* Output pins initial state (LOW) */
    HAL_GPIO_WritePin(GPIOA, OUT_GPIOA_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, OUT_GPIOB_PINS, GPIO_PIN_RESET);

    /* Output pins configuration (PA8, PB12..PB15) */
    gpio_init.Pin = OUT_GPIOA_PINS;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = OUT_GPIOB_PINS;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* 5 Buttons: PA5..PA7 (EXTI9_5), PB0 (EXTI0), PB1 (EXTI1) */
    gpio_init.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Pin = BTN_GPIOA_PINS;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = BTN_GPIOB_PINS;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* Interrupt Priorities */
    HAL_NVIC_SetPriority(DHT11_EXTI_IRQn, DHT11_EXTI_PRIO, 0);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(BTN4_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN4_EXTI_IRQn);
    HAL_NVIC_SetPriority(BTN5_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN5_EXTI_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = BT_UART_INSTANCE;
    huart2.Init.BaudRate = BT_UART_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = I2C2_CLOCK_SPEED;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_TIM2_Init(void)
{
    uint32_t timer_clock_hz;

    __HAL_RCC_TIM2_CLK_ENABLE();

    timer_clock_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        timer_clock_hz *= 2u;
    }

    if ((timer_clock_hz % TIM2_COUNTER_FREQ_HZ) != 0u) {
        Error_Handler();
    }

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (timer_clock_hz / TIM2_COUNTER_FREQ_HZ) - 1u;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 65535u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

/*==================== MSP Implementations ====================*/

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance == BT_UART_INSTANCE) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART2_CLK_ENABLE();

        gpio_init.Pin = BT_UART_TX_PIN;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BT_UART_TX_PORT, &gpio_init);

        gpio_init.Pin = BT_UART_RX_PIN;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(BT_UART_RX_PORT, &gpio_init);

        HAL_NVIC_SetPriority(BT_UART_IRQn, BT_UART_PRIO, 0);
        HAL_NVIC_EnableIRQ(BT_UART_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(BT_UART_TX_PORT, BT_UART_TX_PIN);
        HAL_GPIO_DeInit(BT_UART_RX_PORT, BT_UART_RX_PIN);
        HAL_NVIC_DisableIRQ(BT_UART_IRQn);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C2) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init.Pin = I2C2_SCL_PIN | I2C2_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(I2C2_SCL_PORT, &gpio_init);

        __HAL_RCC_I2C2_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_DISABLE();
        HAL_GPIO_DeInit(I2C2_SCL_PORT, I2C2_SCL_PIN | I2C2_SDA_PIN);
    }
}

/*==================== HAL Interrupt Callbacks ====================*/

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        /* User UART RX ISR handler */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        (void)huart2.Instance->DR;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t exti_pin)
{
    /* User EXTI Callback (DHT11 or buttons) */
    DHT11_CallbackEXTI(exti_pin);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        DHT11_CallbackTIM2();
    }
}

/*==================== Error Trap ====================*/

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
```

- [ ] **Step 2: Commit `main.c` changes**

```bash
git add stm-firmware/src/main.c
git commit -m "refactor(main): reset main to hardware initialization and interrupt callback skeleton"
```

---

### Task 4: Compilation and Build Verification

**Files:**
- Read: `stm-firmware/build/`

**Interfaces:**
- Consumes: All source files and headers
- Produces: `firmware.elf`, `firmware.bin`, `firmware.hex`

- [ ] **Step 1: Run CMake build**

Run build command in `stm-firmware`:
```bash
cmake --build build
```
Expected: Build finishes with exit code 0 and outputs `firmware.elf`, `firmware.hex`, `firmware.bin`.

- [ ] **Step 2: Check git status and confirm branch state**

```bash
git status
```
Expected: Clean working tree on branch `refactor/clean-hardware-scaffold`.

---
