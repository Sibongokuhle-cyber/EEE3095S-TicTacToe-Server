"""Automated protocol tests for the STM32 Tic-Tac-Toe server (no human interaction).

Reference protocol (ASCII, '\\n' terminated, no spaces):
  CONNECT -> OK                       NEW -> OK:BOARD:.........
  MOVE:<i>[:<X|O>] -> OK:BOARD:<9> | WIN:X | WIN:O | DRAW
                      | NACK:OCCUPIED | NACK:RANGE | NACK:NOTURN | NACK:NOGAME | NACK:BADCMD
  STATE -> BOARD:<9> | NACK:NOGAME    RESIGN -> OK:GAMEOVER | NACK:NOGAME
  DISCONNECT -> OK
  Extra (documented) errors: NACK:NOCONN, NACK:TOOLONG, NACK:TIMEOUT
"""
import time

from conftest import send_cmd, send_raw, read_line

EMPTY = "........."


# ------------------------------------------------------------------ helpers
def place(board, idx, mark):
    return board[:idx] + mark + board[idx + 1:]


def connect(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser, "OK") == "OK"


def new_game(ser):
    connect(ser)
    send_cmd(ser, "NEW")
    assert read_line(ser, "OK:BOARD:" + EMPTY) == "OK:BOARD:" + EMPTY


def play(ser, moves):
    """Play alternating X/O moves; assert each non-terminal reply; return final board."""
    board = EMPTY
    for n, idx in enumerate(moves[:-1]):
        board = place(board, idx, "XO"[n % 2])
        send_cmd(ser, f"MOVE:{idx}")
        assert read_line(ser) == f"OK:BOARD:{board}"
    return place(board, moves[-1], "XO"[(len(moves) - 1) % 2])


def state_is(ser, board):
    send_cmd(ser, "STATE")
    assert read_line(ser) == f"BOARD:{board}"


# -------------------------------------------------------- required behaviour
def test_connect_returns_ok(ser):
    connect(ser)


def test_new_returns_empty_board(ser):
    new_game(ser)


def test_legal_move_changes_board_exactly(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:4")
    assert read_line(ser) == "OK:BOARD:....X...."
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK:BOARD:O...X...."
    state_is(ser, "O...X....")


def test_occupied_move_rejected_and_board_unchanged(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK:BOARD:X........"
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "NACK:OCCUPIED"
    state_is(ser, "X........")


def test_out_of_range_rejected(ser):
    new_game(ser)
    for bad in ("9", "10", "99", "-1", "123456789012345"):
        send_cmd(ser, f"MOVE:{bad}")
        assert read_line(ser) == "NACK:RANGE", bad
    state_is(ser, EMPTY)


def test_move_after_game_over_rejected(ser):
    new_game(ser)
    final = play(ser, [0, 3, 1, 4, 2])          # X wins on the top row
    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN:X"
    send_cmd(ser, "MOVE:5")
    assert read_line(ser) == "NACK:NOGAME"
    state_is(ser, final)                         # STATE still allowed, board final


def test_scripted_win_x(ser):
    new_game(ser)
    play(ser, [0, 3, 1, 4, 2])
    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN:X"


def test_scripted_win_o(ser):
    new_game(ser)
    play(ser, [0, 3, 1, 4, 8, 5])               # O: 3,4,5 -> middle row
    send_cmd(ser, "MOVE:5")
    assert read_line(ser) == "WIN:O"


def test_scripted_draw(ser):
    new_game(ser)
    seq = [0, 1, 2, 4, 3, 5, 7, 6, 8]           # X O X / X O O / O X X
    play(ser, seq)
    send_cmd(ser, "MOVE:8")
    assert read_line(ser) == "DRAW"
    state_is(ser, "XOXXOOOXX")


def test_new_after_game_over_starts_fresh(ser):
    new_game(ser)
    play(ser, [0, 3, 1, 4, 2])
    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN:X"
    send_cmd(ser, "NEW")
    assert read_line(ser) == "OK:BOARD:" + EMPTY


# ------------------------------------------------------------ state machine
def test_before_connect_commands_rejected(ser):
    send_cmd(ser, "NEW")
    assert read_line(ser) == "NACK:NOCONN"
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "NACK:NOCONN"


def test_idle_has_no_game(ser):
    connect(ser)
    for cmd in ("MOVE:0", "STATE", "RESIGN"):
        send_cmd(ser, cmd)
        assert read_line(ser) == "NACK:NOGAME", cmd


def test_new_during_game_rejected_state_unchanged(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:4")
    assert read_line(ser) == "OK:BOARD:....X...."
    send_cmd(ser, "NEW")
    assert read_line(ser) == "NACK:BADCMD"
    state_is(ser, "....X....")


def test_out_of_turn_rejected(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:0:O")                    # X is to move
    assert read_line(ser) == "NACK:NOTURN"
    state_is(ser, EMPTY)
    send_cmd(ser, "MOVE:0:X")
    assert read_line(ser) == "OK:BOARD:X........"
    send_cmd(ser, "MOVE:1:X")                    # now O is to move
    assert read_line(ser) == "NACK:NOTURN"
    state_is(ser, "X........")


def test_resign(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:4")
    assert read_line(ser) == "OK:BOARD:....X...."
    send_cmd(ser, "RESIGN")
    assert read_line(ser) == "OK:GAMEOVER"
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "NACK:NOGAME"
    state_is(ser, "....X....")


def test_disconnect_ends_session(ser):
    new_game(ser)
    send_cmd(ser, "DISCONNECT")
    assert read_line(ser) == "OK"
    send_cmd(ser, "NEW")
    assert read_line(ser) == "NACK:NOCONN"


# ------------------------------------------- malformed / garbage / robustness
def test_unknown_and_wrong_case_commands(ser):
    connect(ser)
    for cmd in ("HELLO", "new", "Connect", "MOVE", "NEWGAME"):
        send_cmd(ser, cmd)
        assert read_line(ser) == "NACK:BADCMD", cmd


def test_malformed_move_does_not_touch_state(ser):
    new_game(ser)
    for bad in ("MOVE:abc", "MOVE:", "MOVE:1x", "MOVE:1:Z", "MOVE:1:XO", "MOVE:1:", "MOVE:--1"):
        send_cmd(ser, bad)
        assert read_line(ser) == "NACK:BADCMD", bad
    state_is(ser, EMPTY)                         # atoi() would have played cell 0


def test_crlf_terminator_gives_single_reply(ser):
    connect(ser)
    send_raw(ser, b"NEW\r\n")
    assert read_line(ser) == "OK:BOARD:" + EMPTY
    send_cmd(ser, "STATE")
    assert read_line(ser) == "BOARD:" + EMPTY    # no stray second reply in between


def test_frame_split_across_writes(ser):
    new_game(ser)
    send_raw(ser, b"MO")
    time.sleep(0.1)
    send_raw(ser, b"VE:0\n")
    assert read_line(ser) == "OK:BOARD:X........"


def test_oversized_frame_rejected_and_tail_not_executed(ser):
    connect(ser)
    send_raw(ser, b"A" * 100 + b"NEW\n")         # tail looks like a valid command
    assert read_line(ser) == "NACK:TOOLONG"
    send_cmd(ser, "STATE")
    assert read_line(ser) == "NACK:NOGAME"       # NEW must NOT have run
    send_raw(ser, b"B" * 500 + b"\n")            # larger than the RX ring
    assert read_line(ser) in ("NACK:TOOLONG", "NACK:BADCMD")
    connect(ser)                                 # link still alive


def test_garbage_bytes_rejected_then_recovers(ser):
    connect(ser)
    send_raw(ser, b"\x00\xff\x01NEW\n")
    assert read_line(ser) == "NACK:BADCMD"
    send_cmd(ser, "STATE")
    assert read_line(ser) == "NACK:NOGAME"       # NEW inside the garbage line did not run
    send_cmd(ser, "NEW")
    assert read_line(ser) == "OK:BOARD:" + EMPTY


def test_partial_frame_times_out_and_is_discarded(ser):
    new_game(ser)
    send_raw(ser, b"MOV")                        # no terminator
    assert read_line(ser) == "NACK:TIMEOUT"      # arrives after ~0.5 s (ser.timeout is 2 s)
    send_cmd(ser, "MOVE:0")                      # must not be glued onto "MOV"
    assert read_line(ser) == "OK:BOARD:X........"