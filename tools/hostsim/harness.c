#define _GNU_SOURCE
#include "main.h"
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
int fw_main(void);
volatile int armed = 0;
uint32_t HAL_GetTick(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint32_t)(t.tv_sec*1000+t.tv_nsec/1000000); }
void HAL_Delay(uint32_t ms){ usleep(ms*1000); }
void HAL_Init(void){} 
void HAL_GPIO_WritePin(void*p,uint16_t pin,GPIO_PinState s){} GPIO_PinState HAL_GPIO_ReadPin(void*p,uint16_t pin){return 1;} void HAL_GPIO_Init(void*p,GPIO_InitTypeDef*i){}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*h){return 0;}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*h,uint8_t*d,uint16_t n,uint32_t t){ ssize_t r=write(1,d,n); (void)r; return 0; }

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef*o){return 0;} HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef*c,uint32_t l){return 0;} HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*p){return 0;}
void HAL_UART_MspInit(UART_HandleTypeDef*h){}
static void *feeder(void*a){ unsigned char c; while(read(0,&c,1)==1){ while(!armed) usleep(100); usleep(90); /* ~115200 baud */ uart_rx_push(c);} _exit(0); return 0; }
int main(void){ pthread_t t; pthread_create(&t,0,feeder,0); return fw_main(); }
