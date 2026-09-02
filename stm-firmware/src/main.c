/*
 * main.c — Hardware setup & interrupt skeleton for STM32F103 (Blue Pill).
 */
#include "main.h"

#include "dht11.h"
#include "ssd1306.h"
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
