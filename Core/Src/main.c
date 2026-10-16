/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* ---------------------------------------------------------------------------
 * LED (D9, dual colour): PB10 = RED, PB11 = GREEN on the schematic, active HIGH.
 * On this particular board the two colours show swapped, so BOARD_RG_SWAPPED
 * swaps the pins in software. Set it to 0 on a board with correct colours.
 * ------------------------------------------------------------------------- */
#define BOARD_RG_SWAPPED 1
#if BOARD_RG_SWAPPED
  #define LED_R_PIN  GPIO_PIN_11
  #define LED_G_PIN  GPIO_PIN_10
#else
  #define LED_R_PIN  GPIO_PIN_10
  #define LED_G_PIN  GPIO_PIN_11
#endif
#define LED_R_PORT  GPIOB
#define LED_G_PORT  GPIOB
#define LED_ON      GPIO_PIN_SET
#define LED_OFF     GPIO_PIN_RESET

// Button: SW0 on PA0, pressed = LOW (internal pull-up)
#define BUTTON_PORT GPIOA
#define BUTTON_PIN  GPIO_PIN_0
#define BUTTON_DEBOUNCE_MS 50

// Server state
typedef enum { STATE_IDLE, STATE_IN_PROGRESS, STATE_GAME_OVER } GameState;
GameState game_state = STATE_IDLE;
uint8_t connected = 0;          // session flag, set by CONNECT, cleared by DISCONNECT
char board[9];
char turn = 'X';

// UART receive: ISR -> ring buffer -> main-loop line framer
#define RX_RING_SIZE      128
#define LINE_MAX_LEN      63    // payload chars per frame (terminator excluded)
#define FRAME_TIMEOUT_MS  500   // an unterminated frame is discarded after this
volatile uint8_t  rx_byte;
volatile uint8_t  rx_ring[RX_RING_SIZE];
volatile uint16_t rx_head = 0, rx_tail = 0;
volatile uint8_t  rx_fault = 0; // set by ISR on ring overflow or UART error
char     line_buf[LINE_MAX_LEN + 1];
uint16_t line_len = 0;
uint8_t  frame_err = 0;         // FE_* : frame already known to be bad
uint32_t last_rx_tick = 0;
enum { FE_NONE = 0, FE_TOOLONG, FE_GARBAGE };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void game_new(void);
int  check_win(char p);
int  check_draw(void);
void send_str(const char *s);
void send_board_ok(void);
void send_board_state(void);
void handle_move(const char *args);
void process_command(char *cmd);
void process_rx(void);
void check_frame_timeout(void);
void update_led(void);
void check_button(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // Arm reception FIRST so bytes sent while we boot are not lost.
  game_state = STATE_IDLE;
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);

  // --- Boot indicator: flash LED 3 times ---
  for (int i = 0; i < 3; i++) {
      HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, LED_ON);
      HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, LED_ON);
      HAL_Delay(100);
      HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, LED_OFF);
      HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, LED_OFF);
      HAL_Delay(100);
  }

  // --- Boot message over UART ---
  send_str("\r\n=== Tic-Tac-Toe Server Ready ===\r\n");
  send_str("Waiting for CONNECT...\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  process_rx();
	  check_frame_timeout();
	  update_led();
	  check_button();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ============================ game logic ================================== */
void game_new(void) {
    memset(board, '.', 9);
    turn = 'X';
    game_state = STATE_IN_PROGRESS;
}

int check_win(char p) {
    static const uint8_t lines[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    for (int i = 0; i < 8; i++) {
        if (board[lines[i][0]] == p &&
            board[lines[i][1]] == p &&
            board[lines[i][2]] == p) return 1;
    }
    return 0;
}

int check_draw(void) {
    for (int i = 0; i < 9; i++) if (board[i] == '.') return 0;
    return 1;
}

/* ============================== output ==================================== */
void send_str(const char *s) {
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

void send_board_ok(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "OK:BOARD:%.9s\n", board);
    send_str(buf);
}

void send_board_state(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "BOARD:%.9s\n", board);
    send_str(buf);
}

/* ============================ MOVE handler ================================ *
 * Grammar:  MOVE:<int>            e.g. MOVE:4
 *           MOVE:<int>:<X|O>      optional mark; rejected with NOTURN if it
 *                                 is not the side to move.
 * Anything else after "MOVE:" is NACK:BADCMD. Nothing is changed unless every
 * check passes.                                                              */
void handle_move(const char *p) {
    if (game_state != STATE_IN_PROGRESS) { send_str("NACK:NOGAME\n"); return; }

    int neg = 0, ndigits = 0, idx = 0;
    char mark = 0;

    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') {
        if (idx < 1000) idx = idx * 10 + (*p - '0');   // saturate, no overflow
        p++; ndigits++;
    }
    if (ndigits == 0) { send_str("NACK:BADCMD\n"); return; }

    if (*p == ':') {
        mark = p[1];
        if ((mark != 'X' && mark != 'O') || p[2] != '\0') { send_str("NACK:BADCMD\n"); return; }
    } else if (*p != '\0') {
        send_str("NACK:BADCMD\n"); return;
    }

    if (neg || idx > 8)         { send_str("NACK:RANGE\n");    return; }
    if (mark && mark != turn)   { send_str("NACK:NOTURN\n");   return; }
    if (board[idx] != '.')      { send_str("NACK:OCCUPIED\n"); return; }

    board[idx] = turn;

    if (check_win(turn)) {
        char buf[16];
        game_state = STATE_GAME_OVER;
        snprintf(buf, sizeof(buf), "WIN:%c\n", turn);
        send_str(buf);
        return;
    }
    if (check_draw()) {
        game_state = STATE_GAME_OVER;
        send_str("DRAW\n");
        return;
    }
    turn = (turn == 'X') ? 'O' : 'X';
    send_board_ok();
}

/* ========================= command dispatcher ============================= *
 * IDLE        : CONNECT, NEW
 * IN_PROGRESS : MOVE, STATE, RESIGN
 * GAME_OVER   : NEW, STATE, DISCONNECT
 * Departures (documented): CONNECT is accepted in any state (idempotent), and
 * DISCONNECT is accepted in any state (aborts the game, returns to IDLE).
 * Extra NACKs: NOCONN (no session), TOOLONG, TIMEOUT.                        */
void process_command(char *cmd) {
    if (strcmp(cmd, "CONNECT") == 0) { connected = 1; send_str("OK\n"); return; }

    int known = (strcmp(cmd, "NEW") == 0)   || (strcmp(cmd, "STATE") == 0) ||
                (strcmp(cmd, "RESIGN") == 0) || (strcmp(cmd, "DISCONNECT") == 0) ||
                (strncmp(cmd, "MOVE:", 5) == 0);
    if (!known)     { send_str("NACK:BADCMD\n"); return; }
    if (!connected) { send_str("NACK:NOCONN\n"); return; }

    if (strcmp(cmd, "NEW") == 0) {
        if (game_state == STATE_IN_PROGRESS) { send_str("NACK:BADCMD\n"); return; }
        game_new();
        send_board_ok();
        return;
    }
    if (strcmp(cmd, "STATE") == 0) {
        if (game_state == STATE_IDLE) { send_str("NACK:NOGAME\n"); return; }
        send_board_state();
        return;
    }
    if (strcmp(cmd, "RESIGN") == 0) {
        if (game_state != STATE_IN_PROGRESS) { send_str("NACK:NOGAME\n"); return; }
        game_state = STATE_GAME_OVER;
        send_str("OK:GAMEOVER\n");
        return;
    }
    if (strcmp(cmd, "DISCONNECT") == 0) {
        connected = 0;
        game_state = STATE_IDLE;
        send_str("OK\n");
        return;
    }
    handle_move(cmd + 5);   // "MOVE:" prefix already matched
}

/* ============================== framing =================================== *
 * Frames end at '\n' or '\r' (so CRLF works; the empty second terminator is
 * ignored). Max LINE_MAX_LEN chars. Oversized or non-printable frames are
 * discarded UNTIL the next terminator and answered with exactly one NACK, so
 * the tail of a bad frame can never be executed as a command.               */
void process_rx(void) {
    if (rx_fault) {
        rx_fault = 0;
        if (!frame_err) frame_err = FE_GARBAGE;
    }
    while (rx_tail != rx_head) {
        char c = (char)rx_ring[rx_tail];
        rx_tail = (rx_tail + 1) % RX_RING_SIZE;
        last_rx_tick = HAL_GetTick();

        if (c == '\n' || c == '\r') {
            if (frame_err) {
                send_str(frame_err == FE_TOOLONG ? "NACK:TOOLONG\n" : "NACK:BADCMD\n");
            } else if (line_len > 0) {
                line_buf[line_len] = '\0';
                process_command(line_buf);
            }
            line_len = 0;
            frame_err = FE_NONE;
        } else if (frame_err) {
            /* discarding the rest of a bad frame */
        } else if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) {
            frame_err = FE_GARBAGE;
        } else if (line_len < LINE_MAX_LEN) {
            line_buf[line_len++] = c;
        } else {
            frame_err = FE_TOOLONG;
        }
    }
}

/* Drop a half-received frame if the sender goes quiet. */
void check_frame_timeout(void) {
    if ((line_len > 0 || frame_err) &&
        (HAL_GetTick() - last_rx_tick) > FRAME_TIMEOUT_MS) {
        if (frame_err == FE_TOOLONG)       send_str("NACK:TOOLONG\n");
        else if (frame_err == FE_GARBAGE)  send_str("NACK:BADCMD\n");
        else                               send_str("NACK:TIMEOUT\n");
        line_len = 0;
        frame_err = FE_NONE;
    }
}

/* ============================ UART callbacks ============================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        uint16_t next = (uint16_t)((rx_head + 1) % RX_RING_SIZE);
        if (next == rx_tail) {
            rx_fault = 1;                 // ring full: drop byte, poison frame
        } else {
            rx_ring[rx_head] = rx_byte;
            rx_head = next;
        }
        HAL_UART_Receive_IT(huart, (uint8_t *)&rx_byte, 1);
    }
}

/* Overrun / framing / noise: HAL aborts reception on overrun, which would
 * leave RX dead. Flag the frame as corrupt and re-arm.                       */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        rx_fault = 1;
        huart->ErrorCode = HAL_UART_ERROR_NONE;
        HAL_UART_Receive_IT(huart, (uint8_t *)&rx_byte, 1);   // BUSY is harmless
    }
}

/* ============================ LED and button ============================== */
void update_led(void) {
    static uint32_t last = 0;
    static uint8_t blink = 0;

    if (game_state == STATE_IDLE) {
        HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, LED_OFF);
        HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, LED_OFF);
    }
    else if (game_state == STATE_IN_PROGRESS) {
        if (turn == 'X') {                       // RED = X to move
            HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, LED_ON);
            HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, LED_OFF);
        } else {                                 // GREEN = O to move
            HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, LED_OFF);
            HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, LED_ON);
        }
    }
    else {                                       // GAME_OVER: blink both
        if (HAL_GetTick() - last > 150) {
            last = HAL_GetTick();
            blink = !blink;
            HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, blink ? LED_ON : LED_OFF);
            HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, blink ? LED_ON : LED_OFF);
        }
    }
}

/* Button = local "new game". Only while a PC session exists, and the PC is
 * told with an unsolicited OK:BOARD line so the GUI stays in sync.
 * Non-blocking debounce (no HAL_Delay).                                      */
void check_button(void) {
    static uint8_t last = 1;
    static uint32_t t_edge = 0;
    uint8_t cur = (uint8_t)HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);

    if (last == 1 && cur == 0 && (HAL_GetTick() - t_edge) > BUTTON_DEBOUNCE_MS) {
        t_edge = HAL_GetTick();
        if (connected) {
            game_new();
            send_board_ok();
        }
    }
    last = cur;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
