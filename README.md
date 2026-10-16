# STM32F051 Serial Game Server — Tic-Tac-Toe

**EEE3095S Embedded Systems II · CS Project 2026 · University of Cape Town**

An authoritative game server running on the UCT STM32F051 dev board, plus a thin Tkinter GUI client on the PC, talking over the board's ST-Link virtual COM port. **The STM32 is the referee**: it owns the board, whose turn it is and the game status, validates every request, and returns the authoritative result. The GUI only forwards clicks and redraws what the server confirms.

| | |
|---|---|
| Game | Tic-Tac-Toe (3 × 3) |
| MCU / board | STM32F051R8 on the UCT STM32 Dev Board v1.06 |
| Link | USART1 (PA9 TX / PA10 RX) via ST-Link V2.1 virtual COM port, 115200 8N1 |
| Firmware | C, STM32CubeIDE, HAL (init) + register-level RX interrupt |
| Client | Python 3 + Tkinter + pyserial |
| Tests | pytest + pyserial, 23 automated hardware tests |
| Due | 25 October 23:59 |

**Team:** _<name 1>_, _<name 2>_ — _<task allocation: who did firmware / GUI / tests / report>_

---

## Contents
- [STM32F051 Serial Game Server — Tic-Tac-Toe](#stm32f051-serial-game-server--tic-tac-toe)
  - [Contents](#contents)
  - [1. Repository layout](#1-repository-layout)
  - [2. Hardware and pin map](#2-hardware-and-pin-map)
  - [3. Quick start](#3-quick-start)
    - [3.1 Prerequisites](#31-prerequisites)
    - [3.2 Build and flash the firmware](#32-build-and-flash-the-firmware)
    - [3.3 Run the GUI](#33-run-the-gui)
    - [3.4 Run the automated tests](#34-run-the-automated-tests)
  - [4. System architecture](#4-system-architecture)
  - [5. Server state machine](#5-server-state-machine)
  - [6. Message protocol](#6-message-protocol)
    - [6.1 Framing](#61-framing)
    - [6.2 Commands (GUI → STM)](#62-commands-gui--stm)
    - [6.3 Responses (STM → GUI)](#63-responses-stm--gui)
    - [6.4 `MOVE` validation order](#64-move-validation-order)
    - [6.5 Departures from the reference protocol](#65-departures-from-the-reference-protocol)
  - [7. Firmware design](#7-firmware-design)
  - [8. GUI client design](#8-gui-client-design)
  - [9. Automated tests](#9-automated-tests)
  - [10. Requirements traceability](#10-requirements-traceability)
  - [11. Troubleshooting](#11-troubleshooting)
  - [12. Known limitations and possible improvements](#12-known-limitations-and-possible-improvements)
  - [13. Submission checklist](#13-submission-checklist)
  - [14. References](#14-references)

---

## 1. Repository layout

```text
TicTacToeServer/
├── Core/                              # STM32CubeIDE application code
│   ├── Inc/
│   │   ├── main.h                     #   pin names, Error_Handler prototype (CubeMX generated)
│   │   ├── stm32f0xx_hal_conf.h       #   HAL module selection (CubeMX generated)
│   │   └── stm32f0xx_it.h             #   interrupt handler prototypes (CubeMX generated)
│   ├── Src/
│   │   ├── main.c                     #   ★ server: game logic, protocol, framing, LED, button
│   │   ├── stm32f0xx_it.c             #   ★ SysTick + register-level USART1 RX interrupt handler
│   │   ├── stm32f0xx_hal_msp.c        #   ★ USART1 pin setup + NVIC enable (without it RX never works)
│   │   ├── system_stm32f0xx.c         #   CMSIS system init
│   │   ├── syscalls.c                 #   newlib stubs
│   │   └── sysmem.c                   #   heap (_sbrk)
│   └── Startup/
│       └── startup_stm32f051r8hx.s    #   vector table + reset handler
├── Drivers/                           # ST HAL + CMSIS (vendor code, unmodified)
│   ├── CMSIS/
│   └── STM32F0xx_HAL_Driver/
├── python_gui/
│   ├── gui_client.py                  # ★ Tkinter GUI client
│   └── boot_test.py                   #   quick "is the board talking?" link check
├── tests/
│   ├── conftest.py                    # ★ pytest fixture: finds the board, resets it to a known state
│   └── test_protocol.py               # ★ 23 automated protocol / game-logic / robustness tests
├── report/                            #   IEEE-format report (PDF + source) goes here
├── TicTacToeServer.ioc                # CubeMX configuration (pins, clocks, USART1)
├── STM32F051R8HX_FLASH.ld             # linker script
├── TicTacToeServer Debug.launch       # CubeIDE debug configuration
├── .project / .cproject / .mxproject  # CubeIDE project files (needed to import the project)
├── .settings/                         # CubeIDE workspace settings
├── .gitignore
└── README.md
```

★ = files you will actually read or edit. Folders `Debug/`, `EWARM/` and `STM32CubeIDE/` are build output or generator leftovers and are not needed to build.

---

## 2. Hardware and pin map

Everything used is on the UCT dev board; **no extra wiring and no USB-serial adapter**. One USB cable provides power, programming/debugging and the serial link.

| Function | MCU pin | Notes |
|---|---|---|
| USART1 TX → PC | **PA9** | to ST-Link `VCP_RX` |
| USART1 RX ← PC | **PA10** | from ST-Link `VCP_TX` |
| LED D9 **red** | **PB10** | via 150 Ω, active HIGH |
| LED D9 **green** | **PB11** | via 150 Ω, active HIGH |
| Button **SW0** | **PA0** | internal pull-up, pressed = LOW |
| SWD (programming) | PA13 / PA14 | handled by ST-Link |

Clock: HSI 8 MHz, no PLL (SYSCLK = HCLK = PCLK = 8 MHz). USART1 BRR gives about 115942 baud (0.64 % error), well within tolerance.

**Hardware behaviour**

| Server state | LED D9 | Button SW0 |
|---|---|---|
| IDLE | off | ignored |
| IN_PROGRESS, X to move | **red** | local "new game" (only while a PC session exists) |
| IN_PROGRESS, O to move | **green** | local "new game" |
| GAME_OVER | red + green **blinking** (150 ms) | local "new game" |
| Boot | 3 flashes of both colours | – |

> **LED colours on this board:** on the author's board the D9 red/green dies are physically swapped. `BOARD_RG_SWAPPED` at the top of `main.c` compensates in software. Set it to `0` on a board whose colours are correct.

When the button starts a game, the board also sends an **unsolicited** `OK:BOARD:.........` line so the GUI stays in sync.

---

## 3. Quick start

### 3.1 Prerequisites
- STM32CubeIDE (HAL package for STM32F0 is bundled)
- Python 3.9+ with Tkinter (included with the python.org installer)
- `pip install pyserial pytest`
- UCT STM32 dev board connected by USB; find its COM port in Device Manager → *Ports (COM & LPT)* → "STMicroelectronics STLink Virtual COM Port"

### 3.2 Build and flash the firmware
1. STM32CubeIDE → **File ▸ Import ▸ General ▸ Existing Projects into Workspace** → select this folder.
2. **Project ▸ Build Project** (configuration `Debug`).
3. **Run ▸ Debug** (or *Run*) using `TicTacToeServer Debug.launch`.
4. On reset the LED flashes 3 times, then the board prints:
   ```
   === Tic-Tac-Toe Server Ready ===
   Waiting for CONNECT...
   ```
   Check it with `python python_gui/boot_test.py COM14`.

### 3.3 Run the GUI
```powershell
cd python_gui
python gui_client.py
```
Pick the COM port → **Connect** → **New Game** → click cells. Close any other program using the port first (serial ports are exclusive).

### 3.4 Run the automated tests
Close the GUI first, then:
```powershell
cd tests
$env:SERIAL_PORT = "COM14"          # your port
python -m pytest -v test_protocol.py
```
Linux/macOS: `export SERIAL_PORT=/dev/ttyACM0 && pytest -v tests`.
Without `SERIAL_PORT` the tests are skipped (so they never fail on a machine with no board).

---

## 4. System architecture

```text
 ┌──────────────────────────┐   USB (ST-Link V2.1 VCP)   ┌────────────────────────────────┐
 │ PC                       │◄──────────────────────────►│ STM32F051R8  (SERVER)          │
 │  gui_client.py  (CLIENT) │   ASCII lines, 115200 8N1  │  USART1 RX ISR ─► ring buffer  │
 │   • sends requests       │                            │     ─► line framer ─► parser   │
 │   • draws server replies │                            │     ─► state machine + rules   │
 │  pytest (test harness)   │                            │     ─► reply on USART1 TX      │
 └──────────────────────────┘                            │  LED D9 (turn / game over)     │
                                                         │  Button SW0 (new game)         │
                                                         └────────────────────────────────┘
```

**Why the server owns everything (client-untrusted design).** The GUI never decides whether a move is legal and never infers state: a click becomes `MOVE:<i>`, and the board is redrawn only from a valid `OK:BOARD:` / `BOARD:` reply. This keeps a single source of truth, makes the server testable without a GUI, and means a buggy or malicious client cannot corrupt the game.

**Request/response flow (one request in flight):**

```mermaid
sequenceDiagram
    participant G as GUI / pytest
    participant S as STM32 server
    G->>S: CONNECT
    S-->>G: OK
    G->>S: NEW
    S-->>G: OK:BOARD:.........
    G->>S: MOVE:4
    S-->>G: OK:BOARD:....X....
    G->>S: MOVE:4
    S-->>G: NACK:OCCUPIED
    Note over G,S: ...moves continue...
    G->>S: MOVE:2
    S-->>G: WIN:X
    G->>S: STATE
    S-->>G: BOARD:XXXOO....
    G->>S: DISCONNECT
    S-->>G: OK
```

---

## 5. Server state machine

Three game states plus an orthogonal **session** flag (`connected`, set by `CONNECT`, cleared by `DISCONNECT`).

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> IN_PROGRESS: NEW (OK:BOARD) / button
    IN_PROGRESS --> IN_PROGRESS: MOVE legal (OK:BOARD)
    IN_PROGRESS --> IN_PROGRESS: MOVE illegal (NACK, state unchanged)
    IN_PROGRESS --> GAME_OVER: MOVE wins (WIN:X/O) or fills board (DRAW)
    IN_PROGRESS --> GAME_OVER: RESIGN (OK:GAMEOVER)
    GAME_OVER --> IN_PROGRESS: NEW (OK:BOARD) / button
    GAME_OVER --> GAME_OVER: STATE / MOVE (NACK:NOGAME)
    IN_PROGRESS --> IDLE: DISCONNECT
    GAME_OVER --> IDLE: DISCONNECT
```

**Which command is accepted where**

| Command | IDLE | IN_PROGRESS | GAME_OVER | No session |
|---|---|---|---|---|
| `CONNECT` | OK | OK¹ | OK¹ | OK (opens session) |
| `NEW` | OK:BOARD | `NACK:BADCMD` | OK:BOARD | `NACK:NOCONN` |
| `MOVE:…` | `NACK:NOGAME` | validated (see §6.4) | `NACK:NOGAME` | `NACK:NOCONN` |
| `STATE` | `NACK:NOGAME` | `BOARD:…` | `BOARD:…` | `NACK:NOCONN` |
| `RESIGN` | `NACK:NOGAME` | `OK:GAMEOVER` | `NACK:NOGAME` | `NACK:NOCONN` |
| `DISCONNECT` | OK | OK¹ | OK | `NACK:NOCONN` |
| anything else | `NACK:BADCMD` | `NACK:BADCMD` | `NACK:BADCMD` | `NACK:BADCMD` |

¹ Departure from the brief (justified): `CONNECT` is idempotent so a GUI can reconnect after a crash, and `DISCONNECT` aborts a running game so closing the GUI mid-game always returns the server to IDLE.

---

## 6. Message protocol

### 6.1 Framing
- ASCII, **case-sensitive**, **no spaces**.
- A frame ends at `\n` or `\r` (so `\r\n` works; the empty second terminator is ignored).
- **Maximum 63 characters** per frame (terminator excluded).
- Only printable ASCII (0x20–0x7E) is accepted inside a frame.
- Oversized or non-printable frames are **discarded up to the next terminator** and answered with exactly one NACK, so the tail of a bad frame can never run as a command.
- An unterminated frame is **discarded after 500 ms** of silence and answered with `NACK:TIMEOUT`.
- Board encoding: 9 characters, index 0–8 left-to-right, top-to-bottom; `.` empty, `X`, `O`. X always moves first.

```text
 0 | 1 | 2
---+---+---
 3 | 4 | 5          "XO.XO...."  =  X O .
---+---+---                         X O .
 6 | 7 | 8                          . . .
```

### 6.2 Commands (GUI → STM)

| Command | Meaning |
|---|---|
| `CONNECT` | open a session |
| `NEW` | start a new game, X to move |
| `MOVE:<index>` | place the side-to-move's mark at 0–8 |
| `MOVE:<index>:<X\|O>` | same, but the server rejects it with `NACK:NOTURN` if the mark is not the side to move |
| `STATE` | return the current board |
| `RESIGN` | forfeit the current game |
| `DISCONNECT` | end the session |

### 6.3 Responses (STM → GUI)

| Response | When |
|---|---|
| `OK` | `CONNECT`, `DISCONNECT` accepted |
| `OK:BOARD:<9>` | `NEW` accepted; legal non-terminal `MOVE`; button-initiated new game (unsolicited) |
| `BOARD:<9>` | reply to `STATE` |
| `WIN:X` / `WIN:O` | the move completed three in a row (game now GAME_OVER) |
| `DRAW` | the move filled the board with no winner (GAME_OVER) |
| `OK:GAMEOVER` | `RESIGN` accepted |
| `NACK:OCCUPIED` | target cell already taken |
| `NACK:RANGE` | index not in 0–8 (including negative or very large numbers) |
| `NACK:NOTURN` | optional mark does not match the side to move |
| `NACK:NOGAME` | command needs a running game (or `MOVE` after game over) |
| `NACK:BADCMD` | unknown, malformed, non-printable or out-of-state command |
| `NACK:NOCONN` | *(extra)* command sent before `CONNECT` |
| `NACK:TOOLONG` | *(extra)* frame longer than 63 characters |
| `NACK:TIMEOUT` | *(extra)* frame not terminated within 500 ms |

On `WIN`/`DRAW` the server does **not** send the board; the client asks with `STATE` (allowed in GAME_OVER) and redraws from that reply.

### 6.4 `MOVE` validation order
All checks happen **before** anything is modified, so a rejected request can never change state:

1. no game in progress → `NACK:NOGAME`
2. not `MOVE:<digits>[:<X|O>]` → `NACK:BADCMD` (so `MOVE:abc`, `MOVE:`, `MOVE:1x`, `MOVE:1:Z` change nothing)
3. negative or > 8 → `NACK:RANGE`
4. mark given and ≠ side to move → `NACK:NOTURN`
5. cell not empty → `NACK:OCCUPIED`
6. otherwise place the mark, then: win → `WIN:<m>`, full board → `DRAW`, else swap turn and `OK:BOARD:<9>`

### 6.5 Departures from the reference protocol
- Extra error codes `NOCONN`, `TOOLONG`, `TIMEOUT` (specific errors for specific faults).
- Optional `:<X|O>` suffix on `MOVE` so out-of-turn requests can be detected (`NACK:NOTURN`) even though the single GUI plays both sides.
- `CONNECT`/`DISCONNECT` accepted in any state (see §5).
- Unsolicited `OK:BOARD:.........` when the hardware button starts a game.
- No checksum (optional in the brief).

---

## 7. Firmware design

All server logic is in `Core/Src/main.c` (inside the CubeMX `USER CODE` blocks, so regenerating from the `.ioc` keeps it).

**Receive path**
```text
USART1 RXNE interrupt ─► USART1_IRQHandler (stm32f0xx_it.c)
        ├─ error flags (ORE/FE/NE): clear, set rx_fault
        └─ uart_rx_push(byte) ─► 128-byte ring buffer (head written by ISR only)
main loop ─► process_rx(): ring ─► line framer ─► process_command() ─► handle_move()
```
- **Register-level ISR, not HAL.** `HAL_UART_Receive_IT` re-armed per byte is too slow at 8 MHz / 115200 baud and overruns on back-to-back bytes (seen on hardware as long frames answered `BADCMD`). The ISR is a few dozen cycles.
- **NVIC enable** is in `HAL_UART_MspInit` (`HAL_NVIC_EnableIRQ(USART1_IRQn)`). Without it the receive interrupt never fires and the board is deaf. If you enable the interrupt in CubeMX instead, delete `USART1_IRQHandler` from `stm32f0xx_it.c`.
- **Ring overflow or UART error** sets `rx_fault`; the framer then rejects the current frame rather than acting on possibly corrupt data.
- **Boot:** reception is armed *before* the 3-flash boot indicator so early bytes aren't lost.
- **Non-blocking loop:** `process_rx()`, `check_frame_timeout()`, `update_led()`, `check_button()` run in a tight loop. The button is debounced with timestamps (50 ms), not `HAL_Delay`.
- **Transmit** is polled (`HAL_UART_Transmit` with a 100 ms timeout), so it can never block forever.
- **Safe parsing:** no `atoi`; digits are parsed manually with saturation so huge numbers can't overflow.

---

## 8. GUI client design

`python_gui/gui_client.py` (Tkinter, pyserial, one reader thread feeding a queue polled by Tk every 50 ms).

- **Handshake:** after opening the port it resends `CONNECT` every 600 ms (up to ~9 s) until the board answers `OK`, so it works whether or not opening the port reset the board.
- **Opens the port with DTR/RTS low** to avoid pulsing reset where the driver allows it.
- **One request in flight:** after a request all board clicks are ignored until the reply arrives (or 2 s pass → "No reply from board").
- **Redraw only from the server:** `OK:BOARD:` and `BOARD:` replies are the only things that change the board. After `WIN`/`DRAW` it sends `STATE` and redraws from the answer.
- **Specific rejection messages:** each `NACK:*` shows a dialog with a human explanation.
- **Controls are derived from state** in one function (`_set_controls`): board cells only while connected and a game is live; New Game only when connected; Resign only during a game.
- **Clean disconnect:** sends `DISCONNECT`, closes the port, also on window close.
- **Button-initiated games** (unsolicited `OK:BOARD:`) are picked up automatically.

---

## 9. Automated tests

`tests/conftest.py` opens `SERIAL_PORT`, **syncs by sending `CONNECT` until the board says `OK`** (it does not depend on seeing the boot banner or on DTR resetting the MCU), then sends `DISCONNECT` so **every test starts from IDLE with no session**.

| Group | Test | Checks |
|---|---|---|
| Required | `test_connect_returns_ok` | `CONNECT` → `OK` |
| | `test_new_returns_empty_board` | `NEW` → `OK:BOARD:.........` |
| | `test_legal_move_changes_board_exactly` | move changes only the chosen cell; `STATE` agrees |
| | `test_occupied_move_rejected_and_board_unchanged` | `NACK:OCCUPIED`, board unchanged |
| | `test_out_of_range_rejected` | 9, 10, 99, −1, 15-digit number → `NACK:RANGE` |
| | `test_move_after_game_over_rejected` | `NACK:NOGAME`; `STATE` still returns final board |
| | `test_scripted_win_x` / `test_scripted_win_o` | scripted wins → `WIN:X` / `WIN:O` |
| | `test_scripted_draw` | full-board draw → `DRAW`, final board correct |
| | `test_new_after_game_over_starts_fresh` | `NEW` after game over → empty board |
| State machine | `test_before_connect_commands_rejected` | `NACK:NOCONN` |
| | `test_idle_has_no_game` | `MOVE`/`STATE`/`RESIGN` in IDLE → `NACK:NOGAME` |
| | `test_new_during_game_rejected_state_unchanged` | `NACK:BADCMD`, game intact |
| | `test_out_of_turn_rejected` | `MOVE:0:O` on X's turn → `NACK:NOTURN`, state unchanged |
| | `test_resign` | `OK:GAMEOVER`, then `NACK:NOGAME`, `STATE` still works |
| | `test_disconnect_ends_session` | `OK`, then `NACK:NOCONN` |
| Robustness | `test_unknown_and_wrong_case_commands` | `HELLO`, `new`, `Connect`, `MOVE`, `NEWGAME` → `NACK:BADCMD` |
| | `test_malformed_move_does_not_touch_state` | `MOVE:abc`, `MOVE:`, `MOVE:1x`, `MOVE:1:Z`… → `BADCMD`, board still empty |
| | `test_crlf_terminator_gives_single_reply` | `\r\n` produces exactly one reply |
| | `test_frame_split_across_writes` | `MO` … `VE:0\n` works |
| | `test_oversized_frame_rejected_and_tail_not_executed` | 100-char frame ending in `NEW` → `NACK:TOOLONG`, `NEW` did **not** run; 500-char burst survives |
| | `test_garbage_bytes_rejected_then_recovers` | `\x00\xff\x01NEW` → `NACK:BADCMD`, server still works |
| | `test_partial_frame_times_out_and_is_discarded` | `MOV` → `NACK:TIMEOUT`; next `MOVE:0` is not glued onto it |

> Test results for the report: run `python -m pytest -v test_protocol.py` and save the output (or a screenshot) as evidence.

---

## 10. Requirements traceability

| Brief requirement | Where it is met |
|---|---|
| UART link, authoritative STM32 server | `main.c`, USART1 |
| Board, turn and status are the single source of truth | `board[]`, `turn`, `game_state` in `main.c` |
| Reject out-of-range / occupied / out-of-turn / out-of-state | `handle_move()`, `process_command()` (§6.4, §5) |
| Detect all wins and draws, refuse moves after game over | `check_win()`, `check_draw()`, `NACK:NOGAME` |
| Frame and parse safely: partial, malformed, oversized, garbage | `process_rx()`, `check_frame_timeout()`, strict parser (§6.1, §7) |
| Recover without crashing or blocking | non-blocking loop, polled TX with timeout, UART error flags handled |
| IDLE / IN_PROGRESS / GAME_OVER states | §5 |
| On-board button and LED behaviour | `update_led()`, `check_button()` (§2) |
| GUI: connect, new game, move, rejection messages, win/draw, disable after game, disconnect | `gui_client.py` (§8) |
| GUI redraws only from a valid server response | `_set_board()` driven by `OK:BOARD:` / `BOARD:` only |
| Automated pytest harness, no human interaction | `tests/` (§9) |
| Server FSM, GUI flowchart, circuit diagram | §5 (FSM); GUI flowchart and circuit diagram go in the report |

---

## 11. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| GUI says "No reply from board" | wrong COM port, firmware not flashed, or another program (pytest, boot_test, a terminal) holds the port |
| Boot banner appears but commands get no reply | USART1 interrupt not enabled: check `HAL_NVIC_EnableIRQ(USART1_IRQn)` in `HAL_UART_MspInit` |
| Long frames answered `NACK:BADCMD` instead of `RANGE`/`TOOLONG` | UART overruns from a too-slow RX path: make sure `USART1_IRQHandler` is the register-level one in `stm32f0xx_it.c`, not `HAL_UART_IRQHandler` |
| `multiple definition of USART1_IRQHandler` | you also enabled the interrupt in CubeMX: delete the hand-written one or untick the CubeMX option |
| LED shows the wrong colour for X/O | set `BOARD_RG_SWAPPED` to `0` (or `1`) in `main.c` |
| All tests skipped | `SERIAL_PORT` environment variable not set in this shell |
| Tests fail only on the first run after plugging in | the board was still booting; rerun (fixture retries for 10 s) |
| `could not open port 'COM14'` | wrong port number or the port is busy; check Device Manager |
| Git errors or odd file states | avoid keeping the `.git` folder inside OneDrive-synced folders when possible; sync conflicts can corrupt it |

---

## 12. Known limitations and possible improvements

- No per-message checksum/CRC (optional in the brief); a corrupted byte that still forms a valid command would be accepted.
- Single client; the GUI plays both X and O (hot-seat). The server's turn logic and `NOTURN` support two-client play with a small extension.
- Possible extras: AI opponent (must never send an illegal move), hardware-timer move clock, move history/undo.

---

## 13. Submission checklist

- [ ] Report (IEEE conference format, ≤ 10 pages including references) in `report/`
- [ ] Protocol description, server FSM, GUI flowchart, circuit diagram in the report
- [ ] Pytest output / screenshots and a short demo video (link in the report)
- [ ] GitHub repo link in the report
- [ ] `.cproject`, `.project`, `.mxproject`, `.settings/` committed (needed to import into CubeIDE)
- [ ] Fresh-clone test: import, build, flash, run GUI, run pytest on a clean folder
- [ ] Submitted before **25 October 23:59** (10 % penalty per day late)

---

## 14. References

- EEE3095S Serial Game Server project brief (2026)
- STM32F051 Datasheet, Reference Manual (RM0091), Programming Manual (PM0215)
- STM32F0 HAL and Low-Layer drivers user manual (UM1785)
- UCT STM32 Dev Board schematic v1.06 (Justin Pead)
- pySerial documentation — https://pyserial.readthedocs.io
- pytest documentation — https://docs.pytest.org
- Python `tkinter` documentation — https://docs.python.org/3/library/tkinter.html