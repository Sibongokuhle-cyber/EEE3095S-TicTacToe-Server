#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { void *Instance; struct { uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl, OverSampling, OneBitSampling; } Init;
  struct { uint32_t AdvFeatureInit; } AdvancedInit; uint32_t ErrorCode; } UART_HandleTypeDef;
typedef struct { uint32_t OscillatorType, HSIState, HSICalibrationValue; struct { uint32_t PLLState; } PLL; } RCC_OscInitTypeDef;
typedef struct { uint32_t ClockType, SYSCLKSource, AHBCLKDivider, APB1CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t PeriphClockSelection, Usart1ClockSelection; } RCC_PeriphCLKInitTypeDef;
typedef int HAL_StatusTypeDef; typedef int GPIO_PinState;
#define HAL_OK 0
#define USART1 ((void*)1)
#define GPIOA ((void*)10)
#define GPIOB ((void*)11)
#define GPIO_PIN_0 1
#define GPIO_PIN_10 0x400
#define GPIO_PIN_11 0x800
#define GPIO_PIN_9 0x200
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_PULLUP 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
#define __HAL_RCC_GPIOA_CLK_ENABLE()
#define __HAL_RCC_GPIOB_CLK_ENABLE()
extern volatile int armed;
#define UART_IT_RXNE 0
#define __HAL_UART_ENABLE_IT(h,i) (armed=1)
#define __disable_irq()
#define __enable_irq()
#define HAL_UART_ERROR_NONE 0
#define UART_WORDLENGTH_8B 0
#define UART_STOPBITS_1 0
#define UART_PARITY_NONE 0
#define UART_MODE_TX_RX 0
#define UART_HWCONTROL_NONE 0
#define UART_OVERSAMPLING_16 0
#define UART_ONE_BIT_SAMPLE_DISABLE 0
#define UART_ADVFEATURE_NO_INIT 0
#define RCC_OSCILLATORTYPE_HSI 0
#define RCC_HSI_ON 0
#define RCC_HSICALIBRATION_DEFAULT 0
#define RCC_PLL_NONE 0
#define RCC_CLOCKTYPE_HCLK 0
#define RCC_CLOCKTYPE_SYSCLK 0
#define RCC_CLOCKTYPE_PCLK1 0
#define RCC_SYSCLKSOURCE_HSI 0
#define RCC_SYSCLK_DIV1 0
#define RCC_HCLK_DIV1 0
#define FLASH_LATENCY_0 0
#define RCC_PERIPHCLK_USART1 0
#define RCC_USART1CLKSOURCE_PCLK1 0
void HAL_Init(void); void Error_Handler(void);
uint32_t HAL_GetTick(void); void HAL_Delay(uint32_t);
void HAL_GPIO_WritePin(void*, uint16_t, GPIO_PinState); GPIO_PinState HAL_GPIO_ReadPin(void*, uint16_t); void HAL_GPIO_Init(void*, GPIO_InitTypeDef*);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*); HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef*); HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef*, uint32_t); HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*);
void uart_rx_push(uint8_t);
#endif
