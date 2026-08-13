#include "py32f0xx_hal.h"

static UART_HandleTypeDef uart;
static const uint8_t message[] = "Hello World\r\n";

void HAL_MspInit(void)
{
}

void HAL_UART_MspInit(UART_HandleTypeDef *handle)
{
    GPIO_InitTypeDef gpio = {0};
    (void)handle;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void HardFault_Handler(void)
{
    for (;;) {}
}

int main(void)
{
    HAL_Init();
    __HAL_RCC_USART1_CLK_ENABLE();
    uart.Instance = USART1;
    uart.Init.BaudRate = 115200;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    if (HAL_UART_Init(&uart) != HAL_OK) for (;;) {}
    if (HAL_UART_Transmit(&uart, (uint8_t *)message,
                          sizeof(message) - 1u, 1000u) != HAL_OK) for (;;) {}
    __asm volatile ("bkpt #0");
    for (;;) {}
}
