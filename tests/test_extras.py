"""Tests for the three extensions: checksum, AI opponent, move history / undo.

Extension protocol (all optional, the compulsory protocol is unchanged):
  <payload>*<HH>   CRC-8 suffix on any frame; replies carry one if the request did
                   -> NACK:CHECKSUM (nothing executed) on mismatch
  NEW:AI           new game against the built-in AI (human = X, AI = O); the reply
                   to a legal MOVE shows the board after BOTH moves, or WIN/DRAW
  HISTORY          HIST:<i>,<i>,...  (cell index of every move, in order)
  UNDO             remove the last move (last pair in AI mode) -> OK:BOARD:<9>
                   -> NACK:NOUNDO if nothing to undo, NACK:NOGAME if no live game
"""
import random

from conftest import (crc8, frame, read_line, read_raw, send_cmd, send_raw,
                      split_checksum)

EMPTY = "........."
LINES = [(0, 1, 2), (3, 4, 5), (6, 7, 8), (0, 3, 6), (1, 4, 7), (2, 5, 8),
         (0, 4, 8), (2, 4, 6)]


def has_line(board, mark):
    return any(all(board[i] == mark for i in l) for l in LINES)


def connect(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser, "OK") == "OK"


def new_game(ser, ai=False):
    connect(ser)
    send_cmd(ser, "NEW:AI" if ai else "NEW")
    assert read_line(ser) == "OK:BOARD:" + EMPTY


def state_is(ser, board):
    send_cmd(ser, "STATE")
    assert read_line(ser) == f"BOARD:{board}"


def hist_is(ser, text):
    send_cmd(ser, "HISTORY")
    assert read_line(ser) == "HIST:" + text


# ================================================================ checksum
def test_crc8_known_vector():
    assert crc8(b"123456789") == 0xF4


def test_checksummed_request_gets_checksummed_reply(ser):
    send_raw(ser, frame("CONNECT"))
    payload, ok = split_checksum(read_raw(ser))
    assert payload == "OK" and ok is True


def test_plain_request_still_gets_plain_reply(ser):
    send_raw(ser, b"CONNECT\n")
    assert read_raw(ser) == "OK"


def test_reply_mode_follows_each_request(ser):
    send_raw(ser, frame("CONNECT"))
    assert split_checksum(read_raw(ser)) == ("OK", True)
    send_raw(ser, b"CONNECT\n")                      # back to plain
    assert read_raw(ser) == "OK"


def test_checksummed_game_sequence(ser):
    send_raw(ser, frame("CONNECT"))
    assert split_checksum(read_raw(ser)) == ("OK", True)
    send_raw(ser, frame("NEW"))
    assert split_checksum(read_raw(ser)) == ("OK:BOARD:" + EMPTY, True)
    send_raw(ser, frame("MOVE:4"))
    assert split_checksum(read_raw(ser)) == ("OK:BOARD:....X....", True)


def test_wrong_checksum_rejected_and_nothing_executed(ser):
    new_game(ser)
    wrong = crc8(b"MOVE:4") ^ 0x01
    send_raw(ser, f"MOVE:4*{wrong:02X}\n".encode())
    payload, ok = split_checksum(read_raw(ser))
    assert payload == "NACK:CHECKSUM" and ok is True
    state_is(ser, EMPTY)


def test_corrupted_payload_is_detected(ser):
    """A bit error that turns MOVE:4 into another VALID command must not run."""
    new_game(ser)
    corrupted = frame("MOVE:4").replace(b"MOVE:4", b"MOVE:5")   # old checksum kept
    send_raw(ser, corrupted)
    assert split_checksum(read_raw(ser))[0] == "NACK:CHECKSUM"
    state_is(ser, EMPTY)


def test_malformed_suffix_is_not_a_checksum(ser):
    connect(ser)
    for bad in (b"NEW*ZZ\n", b"NEW*1\n", b"NEW*\n"):
        send_raw(ser, bad)
        assert read_line(ser) == "NACK:BADCMD", bad


def test_lowercase_hex_checksum_accepted(ser):
    send_raw(ser, f"CONNECT*{crc8(b'CONNECT'):02x}\n".encode())
    assert split_checksum(read_raw(ser)) == ("OK", True)


# ============================================================ AI opponent
def test_ai_first_reply_has_exactly_one_x_and_one_o(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "MOVE:4")
    reply = read_line(ser)
    assert reply.startswith("OK:BOARD:")
    board = reply[9:]
    assert board[4] == "X" and board.count("X") == 1 and board.count("O") == 1


def test_ai_blocks_a_win(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK:BOARD:X...O...."          # only drawing reply: centre
    send_cmd(ser, "MOVE:1")                                # X threatens cell 2
    assert read_line(ser) == "OK:BOARD:XXO.O...."          # AI must block at 2


def test_ai_takes_a_win(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "MOVE:0"); assert read_line(ser) == "OK:BOARD:X...O...."
    send_cmd(ser, "MOVE:1"); assert read_line(ser) == "OK:BOARD:XXO.O...."
    send_cmd(ser, "MOVE:3")                                # ignores the O threat at 6
    assert read_line(ser) == "WIN:O"
    state_is(ser, "XXOXO.O..")


def test_ai_never_makes_an_illegal_move_and_never_loses(ser):
    rng = random.Random(2026)
    for _ in range(20):
        new_game(ser, ai=True)
        board = EMPTY
        while True:
            idx = rng.choice([i for i, c in enumerate(board) if c == "."])
            send_cmd(ser, f"MOVE:{idx}")
            reply = read_line(ser)
            if reply.startswith("OK:BOARD:"):
                new = reply[9:]
                changed = [i for i in range(9) if new[i] != board[i]]
                assert new[idx] == "X"
                ai_cells = [i for i in changed if i != idx]
                assert len(ai_cells) == 1                      # exactly one AI move
                assert board[ai_cells[0]] == "." and new[ai_cells[0]] == "O"
                assert not has_line(new, "X") and not has_line(new, "O")
                board = new
                continue
            send_cmd(ser, "STATE")
            final = read_line(ser)
            assert final.startswith("BOARD:")
            fb = final[6:]
            assert fb[idx] == "X"
            assert reply != "WIN:X", "the AI lost a game"
            if reply == "WIN:O":
                assert has_line(fb, "O") and not has_line(fb, "X")
            else:
                assert reply == "DRAW"
                assert "." not in fb and not has_line(fb, "X") and not has_line(fb, "O")
            break


def test_ai_mode_rejects_an_out_of_turn_mark(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "MOVE:0:O")
    assert read_line(ser) == "NACK:NOTURN"
    state_is(ser, EMPTY)


def test_new_ai_during_a_game_is_rejected(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "NEW:AI")
    assert read_line(ser) == "NACK:BADCMD"
    state_is(ser, EMPTY)


def test_plain_new_after_ai_game_is_two_player_again(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "RESIGN")
    assert read_line(ser) == "OK:GAMEOVER"
    send_cmd(ser, "NEW")
    assert read_line(ser) == "OK:BOARD:" + EMPTY
    send_cmd(ser, "MOVE:4")
    assert read_line(ser) == "OK:BOARD:....X...."          # no AI reply


# ====================================================== history and undo
def test_history_empty_then_lists_moves_in_order(ser):
    new_game(ser)
    hist_is(ser, "")
    send_cmd(ser, "MOVE:4"); assert read_line(ser) == "OK:BOARD:....X...."
    send_cmd(ser, "MOVE:0"); assert read_line(ser) == "OK:BOARD:O...X...."
    hist_is(ser, "4,0")


def test_history_without_a_game_is_rejected(ser):
    connect(ser)
    send_cmd(ser, "HISTORY")
    assert read_line(ser) == "NACK:NOGAME"


def test_history_still_available_after_game_over(ser):
    new_game(ser)
    for m in (0, 3, 1, 4):
        send_cmd(ser, f"MOVE:{m}"); assert read_line(ser).startswith("OK:BOARD:")
    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN:X"
    hist_is(ser, "0,3,1,4,2")


def test_undo_restores_board_and_turn(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:4"); assert read_line(ser) == "OK:BOARD:....X...."
    send_cmd(ser, "MOVE:0"); assert read_line(ser) == "OK:BOARD:O...X...."
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "OK:BOARD:....X...."
    hist_is(ser, "4")
    send_cmd(ser, "MOVE:1")                                # O is to move again
    assert read_line(ser) == "OK:BOARD:.O..X...."


def test_undo_all_the_way_back_then_nothing_to_undo(ser):
    new_game(ser)
    send_cmd(ser, "MOVE:4"); read_line(ser)
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "OK:BOARD:" + EMPTY
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "NACK:NOUNDO"
    state_is(ser, EMPTY)


def test_undo_without_a_game_or_after_game_over_is_rejected(ser):
    connect(ser)
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "NACK:NOGAME"
    send_cmd(ser, "NEW"); read_line(ser)
    for m in (0, 3, 1, 4, 2):
        send_cmd(ser, f"MOVE:{m}"); read_line(ser)
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "NACK:NOGAME"
    state_is(ser, "XXXOO....")


def test_undo_in_ai_mode_removes_both_moves(ser):
    new_game(ser, ai=True)
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK:BOARD:X...O...."
    hist_is(ser, "0,4")
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "OK:BOARD:" + EMPTY
    hist_is(ser, "")
    send_cmd(ser, "MOVE:8")                                # human is X again
    reply = read_line(ser)
    assert reply.startswith("OK:BOARD:") and reply[9 + 8] == "X"


def test_undo_before_connect_is_rejected(ser):
    send_cmd(ser, "UNDO")
    assert read_line(ser) == "NACK:NOCONN"
