#include <stdio.h>
#include <string.h>
#include "main.h"
extern char board[9]; extern char turn; extern int ai_choose(char); extern int line_win(const char*,char); extern int board_full(const char*);
unsigned long ai_nodes = 0;
volatile int armed = 0;
uint32_t HAL_GetTick(void){return 0;} void HAL_Delay(uint32_t m){} void HAL_Init(void){}
void HAL_GPIO_WritePin(void*p,uint16_t a,GPIO_PinState s){} GPIO_PinState HAL_GPIO_ReadPin(void*p,uint16_t a){return 1;} void HAL_GPIO_Init(void*p,GPIO_InitTypeDef*i){}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*h){return 0;} HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*h,uint8_t*d,uint16_t n,uint32_t t){return 0;}
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef*o){return 0;} HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef*c,uint32_t l){return 0;} HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*p){return 0;}
void HAL_UART_MspInit(UART_HandleTypeDef*h){}
static unsigned long maxn=0, total=0, calls=0, losses=0, games=0, ai_wins=0, draws=0; static int maxply=0;
static void rec(char *b, int ply){   /* b: board with human (X) to move */
  for(int i=0;i<9;i++){ if(b[i]!='.') continue;
    char c[9]; memcpy(c,b,9); c[i]='X';
    if(line_win(c,'X')){ losses++; games++; continue; }
    if(board_full(c)){ draws++; games++; continue; }
    memcpy(board,c,9); ai_nodes=0; int m=ai_choose('O'); unsigned long n=ai_nodes;
    calls++; total+=n; if(n>maxn){maxn=n; maxply=ply;}
    c[m]='O';
    if(line_win(c,'O')){ ai_wins++; games++; continue; }
    if(board_full(c)){ draws++; games++; continue; }
    rec(c, ply+2);
  }
}
int main(void){ char b[9]; memset(b,'.',9); rec(b,0);
  printf("games=%lu  X wins=%lu  O wins=%lu  draws=%lu\n", games, losses, ai_wins, draws);
  printf("AI decisions=%lu  max nodes in one decision=%lu (at ply %d)  mean=%.0f\n", calls, maxn, maxply, (double)total/calls); return 0; }
