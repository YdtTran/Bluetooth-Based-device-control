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
