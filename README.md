# STM32F051 Serial Tic-Tac-Toe Server (EEE3095S 2026)

STM32 is the referee (owns board, turn, state); the Python GUI is a thin client.

## Build / flash
1. STM32CubeIDE -> File -> Import -> Existing Projects into Workspace -> this folder.
2. Build `Debug`, run/debug on the UCT STM32 board (ST-Link virtual COM port, USART1 PA9/PA10, 115200 8N1).
3. LED D9: red = X to move, green = O to move, both blinking = game over, off = idle.
   `BOARD_RG_SWAPPED` in `main.c` compensates for boards whose red/green are swapped.
4. Button SW0 (PA0): local new game (only while a PC session exists).

## GUI
    pip install pyserial
    python python_gui/gui_client.py

## Automated tests (board connected, GUI closed)
    pip install pytest pyserial
    set SERIAL_PORT=COM14            (Windows)   /   export SERIAL_PORT=/dev/ttyACM0
    pytest -v tests

## Protocol (ASCII, newline-terminated, max 63 chars, no spaces)
| GUI -> STM | STM -> GUI |
|---|---|
| `CONNECT` | `OK` |
| `NEW` | `OK:BOARD:.........` |
| `MOVE:<0-8>` or `MOVE:<0-8>:<X\|O>` | `OK:BOARD:<9>` / `WIN:X` / `WIN:O` / `DRAW` / `NACK:OCCUPIED` / `NACK:RANGE` / `NACK:NOTURN` / `NACK:NOGAME` / `NACK:BADCMD` |
| `STATE` | `BOARD:<9>` / `NACK:NOGAME` |
| `RESIGN` | `OK:GAMEOVER` / `NACK:NOGAME` |
| `DISCONNECT` | `OK` |

Departures from the reference (justify in the report): extra errors `NACK:NOCONN`
(no session), `NACK:TOOLONG`, `NACK:TIMEOUT` (frame incomplete for 500 ms);
`CONNECT` and `DISCONNECT` accepted in any state; optional mark on `MOVE` for
out-of-turn detection; unsolicited `OK:BOARD:.........` when the button starts a game.