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
uint8_t ai_mode = 0;            // 1 = built-in AI plays O (human is X)
uint8_t history[9];             // cell index of every move, in order played
uint8_t hist_len = 0;
char    last_mover = 'X';       // mark of the most recent move (for WIN:<m>)
uint8_t reply_crc = 0;          // 1 = replies carry *HH (set when the last frame did)

// UART receive: ISR -> ring buffer -> main-loop line framer
#define RX_RING_SIZE      128
#define LINE_MAX_LEN      63    // payload chars per frame (terminator excluded)
#define FRAME_TIMEOUT_MS  500   // an unterminated frame is discarded after this
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
int  line_win(const char *b, char p);
int  board_full(const char *b);
int  check_win(char p);
int  check_draw(void);
int  apply_move(int idx);
void send_str(const char *s);
void send_board_ok(void);
void send_board_state(void);
void send_move_result(int r);
void handle_move(const char *args);
void handle_undo(void);
void handle_history(void);
int  negamax(char *b, char me, char opp, int depth, int alpha, int beta);
int  ai_choose(char me);
int  ai_reply(void);
uint8_t crc8(const char *s, uint16_t n);
int  strip_checksum(char *buf, uint16_t *len);
void process_command(char *cmd);
void process_rx(void);
void check_frame_timeout(void);
void uart_rx_push(uint8_t b);
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
  // RX is handled by a tiny register-level ISR (USART1_IRQHandler in
  // stm32f0xx_it.c -> uart_rx_push below). HAL's byte-at-a-time IT API is too
  // slow at 8 MHz / 115200 baud and overruns on back-to-back bytes.
  game_state = STATE_IDLE;
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

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
enum { MV_CONTINUE = 0, MV_WIN, MV_DRAW };

void game_new(void) {
    memset(board, '.', 9);
    turn = 'X';
    hist_len = 0;
    game_state = STATE_IN_PROGRESS;
}

static const uint8_t win_lines[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
};

int line_win(const char *b, char p) {
    for (int i = 0; i < 8; i++) {
        if (b[win_lines[i][0]] == p &&
            b[win_lines[i][1]] == p &&
            b[win_lines[i][2]] == p) return 1;
    }
    return 0;
}

int board_full(const char *b) {
    for (int i = 0; i < 9; i++) if (b[i] == '.') return 0;
    return 1;
}

int check_win(char p)  { return line_win(board, p); }
int check_draw(void)   { return board_full(board); }

/* The ONLY function that writes a mark to the board. Used by the human's MOVE
 * and by the AI, so both obey the same rules. The caller has already checked
 * that idx is in range and the cell is empty.                                */
int apply_move(int idx) {
    history[hist_len++] = (uint8_t)idx;
    board[idx] = turn;
    last_mover = turn;
    if (check_win(turn))   { game_state = STATE_GAME_OVER; return MV_WIN; }
    if (board_full(board)) { game_state = STATE_GAME_OVER; return MV_DRAW; }
    turn = (turn == 'X') ? 'O' : 'X';
    return MV_CONTINUE;
}

/* ============================ AI opponent ================================= *
 * Negamax with alpha-beta pruning on a COPY of the board (the real board is
 * only changed by apply_move). Score from the mover's view: win = 10 - depth,
 * draw = 0, loss = -(10 - depth). Cells are tried centre, corners, edges, so
 * the first best move is deterministic and pruning is effective.            */
#ifndef AI_NODE_HOOK
#define AI_NODE_HOOK()   /* host-test hook that counts search nodes; empty on the target */
#endif
static const uint8_t ai_order[9] = {4, 0, 2, 6, 8, 1, 3, 5, 7};

int negamax(char *b, char me, char opp, int depth, int alpha, int beta) {
    int best = -100;
    AI_NODE_HOOK();
    for (int k = 0; k < 9; k++) {
        int i = ai_order[k], sc;
        if (b[i] != '.') continue;
        b[i] = me;
        if (line_win(b, me))     sc = 10 - depth;
        else if (board_full(b))  sc = 0;
        else                     sc = -negamax(b, opp, me, depth + 1, -beta, -alpha);
        b[i] = '.';
        if (sc > best)   best = sc;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

int ai_choose(char me) {
    char b[9];
    char opp = (me == 'X') ? 'O' : 'X';
    int best = -100, best_i = -1;
    memcpy(b, board, 9);
    for (int k = 0; k < 9; k++) {
        int i = ai_order[k], sc;
        if (b[i] != '.') continue;
        b[i] = me;
        if (line_win(b, me))     sc = 10;
        else if (board_full(b))  sc = 0;
        else                     sc = -negamax(b, opp, me, 1, -100, -best);
        b[i] = '.';
        if (sc > best) { best = sc; best_i = i; }
    }
    return best_i;
}

/* The AI plays the side to move. It only ever picks an empty cell, and the
 * cell is re-checked before apply_move, so it cannot make an illegal move.   */
int ai_reply(void) {
    int idx = ai_choose(turn);
    if (idx < 0 || board[idx] != '.') return MV_CONTINUE;
    return apply_move(idx);
}

/* ============================== checksum ================================== *
 * Optional frame suffix "*HH": HH = two hex digits of CRC-8 (poly 0x07, init
 * 0, no reflection, no final XOR) over the payload before the '*'.
 * crc8("123456789") = 0xF4. A frame that carries a valid-looking suffix is
 * verified (CRC_BAD -> NACK:CHECKSUM, nothing executed) and the reply carries
 * a suffix too; a frame without one is accepted and answered without one.   */
enum { CRC_NONE = 0, CRC_OK, CRC_BAD };

uint8_t crc8(const char *s, uint16_t n) {
    uint8_t c = 0;
    while (n--) {
        c ^= (uint8_t)*s++;
        for (int i = 0; i < 8; i++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int strip_checksum(char *buf, uint16_t *len) {
    uint16_t n = *len;
    int hi = -1, lo = -1;
    if (n >= 3 && buf[n - 3] == '*' &&
        (hi = hexval(buf[n - 2])) >= 0 && (lo = hexval(buf[n - 1])) >= 0) {
        buf[n - 3] = '\0';
        *len = (uint16_t)(n - 3);
        reply_crc = 1;
        return (crc8(buf, (uint16_t)(n - 3)) == (uint8_t)((hi << 4) | lo)) ? CRC_OK : CRC_BAD;
    }
    reply_crc = 0;
    return CRC_NONE;
}

/* ============================== output ==================================== */
void send_str(const char *s) {
    size_t n = strlen(s);
    if (reply_crc && n >= 2 && n < 60 && s[n - 1] == '\n') {
        char buf[64];
        memcpy(buf, s, n - 1);
        snprintf(buf + n - 1, 5, "*%02X\n", crc8(s, (uint16_t)(n - 1)));
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)(n + 3), 100);
    } else {
        HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)n, 100);
    }
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

void send_move_result(int r) {
    if (r == MV_WIN) {
        char buf[16];
        snprintf(buf, sizeof(buf), "WIN:%c\n", last_mover);
        send_str(buf);
    } else if (r == MV_DRAW) {
        send_str("DRAW\n");
    } else {
        send_board_ok();
    }
}

/* ============================ MOVE handler ================================ *
 * Grammar:  MOVE:<int>            e.g. MOVE:4
 *           MOVE:<int>:<X|O>      optional mark; rejected with NOTURN if it
 *                                 is not the side to move.
 * Anything else after "MOVE:" is NACK:BADCMD. Nothing is changed unless every
 * check passes. In AI mode the AI answers immediately, so the reply shows the
 * board after BOTH moves (or the final result).                              */
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

    int r = apply_move(idx);                 // first write to the board
    if (r == MV_CONTINUE && ai_mode) r = ai_reply();
    send_move_result(r);
}

/* UNDO: IN_PROGRESS only. Removes the last move (hot-seat) or the last pair of
 * moves (AI mode, so it is the human's turn again). Checks first, then writes. */
void handle_undo(void) {
    uint8_t n = ai_mode ? 2 : 1;
    if (game_state != STATE_IN_PROGRESS) { send_str("NACK:NOGAME\n");  return; }
    if (hist_len < n)                    { send_str("NACK:NOUNDO\n");  return; }
    while (n--) board[history[--hist_len]] = '.';
    turn = (hist_len % 2 == 0) ? 'X' : 'O';
    send_board_ok();
}

/* HISTORY: "HIST:" followed by the cell indices in move order, comma separated
 * ("HIST:4,0,2"; "HIST:" when there are none). Same states as STATE.         */
void handle_history(void) {
    char buf[40];
    int k = 0;
    if (game_state == STATE_IDLE) { send_str("NACK:NOGAME\n"); return; }
    memcpy(buf, "HIST:", 5); k = 5;
    for (int i = 0; i < hist_len; i++) {
        if (i) buf[k++] = ',';
        buf[k++] = (char)('0' + history[i]);
    }
    buf[k++] = '\n';
    buf[k] = '\0';
    send_str(buf);
}

/* ========================= command dispatcher ============================= *
 * IDLE        : CONNECT, NEW, NEW:AI
 * IN_PROGRESS : MOVE, STATE, HISTORY, UNDO, RESIGN
 * GAME_OVER   : NEW, NEW:AI, STATE, HISTORY, DISCONNECT
 * Departures (documented): CONNECT is accepted in any state (idempotent), and
 * DISCONNECT is accepted in any state (aborts the game, returns to IDLE).
 * Extra NACKs: NOCONN, TOOLONG, TIMEOUT, CHECKSUM, NOUNDO.                   */
void process_command(char *cmd) {
    if (strcmp(cmd, "CONNECT") == 0) { connected = 1; send_str("OK\n"); return; }

    int known = (strcmp(cmd, "NEW") == 0)     || (strcmp(cmd, "NEW:AI") == 0) ||
                (strcmp(cmd, "STATE") == 0)   || (strcmp(cmd, "RESIGN") == 0) ||
                (strcmp(cmd, "UNDO") == 0)    || (strcmp(cmd, "HISTORY") == 0) ||
                (strcmp(cmd, "DISCONNECT") == 0) || (strncmp(cmd, "MOVE:", 5) == 0);
    if (!known)     { send_str("NACK:BADCMD\n"); return; }
    if (!connected) { send_str("NACK:NOCONN\n"); return; }

    if (strcmp(cmd, "NEW") == 0 || strcmp(cmd, "NEW:AI") == 0) {
        if (game_state == STATE_IN_PROGRESS) { send_str("NACK:BADCMD\n"); return; }
        ai_mode = (cmd[3] == ':');
        game_new();
        send_board_ok();
        return;
    }
    if (strcmp(cmd, "STATE") == 0) {
        if (game_state == STATE_IDLE) { send_str("NACK:NOGAME\n"); return; }
        send_board_state();
        return;
    }
    if (strcmp(cmd, "HISTORY") == 0) { handle_history(); return; }
    if (strcmp(cmd, "UNDO") == 0)    { handle_undo();    return; }
    if (strcmp(cmd, "RESIGN") == 0) {
        if (game_state != STATE_IN_PROGRESS) { send_str("NACK:NOGAME\n"); return; }
        game_state = STATE_GAME_OVER;
        send_str("OK:GAMEOVER\n");
        return;
    }
    if (strcmp(cmd, "DISCONNECT") == 0) {
        connected = 0;
        ai_mode = 0;
        game_state = STATE_IDLE;
        send_str("OK\n");
        return;
    }
    handle_move(cmd + 5);   // "MOVE:" prefix already matched
}

/* ============================== framing =================================== *
 * Frames end at '\n' or '\r' (so CRLF works; the empty second terminator is
 * ignored). Max LINE_MAX_LEN chars (the "*HH" suffix counts). Oversized or
 * non-printable frames are discarded UNTIL the next terminator and answered
 * with exactly one NACK, so the tail of a bad frame can never be executed.   */
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
                if (strip_checksum(line_buf, &line_len) == CRC_BAD)
                    send_str("NACK:CHECKSUM\n");        // nothing is executed
                else
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

/* ============================ UART receive ================================ *
 * Called from USART1_IRQHandler (stm32f0xx_it.c) for every received byte.
 * Keep it short: it runs at interrupt level.                                 */
void uart_rx_push(uint8_t b) {
    uint16_t next = (uint16_t)((rx_head + 1) % RX_RING_SIZE);
    if (next == rx_tail) {
        rx_fault = 1;                 // ring full: drop byte, poison frame
    } else {
        rx_ring[rx_head] = b;
        rx_head = next;
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
