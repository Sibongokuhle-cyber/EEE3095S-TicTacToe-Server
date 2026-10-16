import pytest
from conftest import send_cmd, read_line


# ---------------------------------------------------------------- helpers

def _connect(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser, "OK") == "OK"


def _new_game(ser):
    send_cmd(ser, "NEW")
    line = read_line(ser, "OK: BOARD: .........")
    assert line == "OK: BOARD: .........", line


# ------------------------------------------------------------------ tests

def test_connect(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser, "OK") == "OK"


def test_new(ser):
    _connect(ser)
    _new_game(ser)


def test_legal_move(ser):
    _connect(ser)
    _new_game(ser)
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK: BOARD: X........"


def test_occupied(ser):
    _connect(ser)
    _new_game(ser)

    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK: BOARD: X........"

    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "NACK: OCCUPIED"

    send_cmd(ser, "STATE")
    assert read_line(ser) == "BOARD: X........"


def test_range(ser):
    _connect(ser)
    _new_game(ser)

    send_cmd(ser, "MOVE:9")
    assert read_line(ser) == "NACK: RANGE"


def test_after_game_over(ser):
    _connect(ser)
    _new_game(ser)

    # X wins: 0,1,2
    for cmd in ["MOVE:0", "MOVE:3", "MOVE:1", "MOVE:4"]:
        send_cmd(ser, cmd)
        read_line(ser)
    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN: X"

    send_cmd(ser, "MOVE:5")
    # Pin this to whatever the firmware actually returns; change if it
    # answers NACK: BADCMD instead.
    assert read_line(ser) == "NACK: NOGAME"


def test_win(ser):
    _connect(ser)
    _new_game(ser)

    expected = [
        ("MOVE:0", "OK: BOARD: X........"),
        ("MOVE:3", "OK: BOARD: X..O....."),
        ("MOVE:1", "OK: BOARD: XX.O....."),
        ("MOVE:4", "OK: BOARD: XX.OO...."),
    ]
    for cmd, reply in expected:
        send_cmd(ser, cmd)
        assert read_line(ser) == reply

    send_cmd(ser, "MOVE:2")
    assert read_line(ser) == "WIN: X"


def test_draw(ser):
    _connect(ser)
    _new_game(ser)

    expected = [
        ("MOVE:0", "OK: BOARD: X........"),
        ("MOVE:1", "OK: BOARD: XO......."),
        ("MOVE:2", "OK: BOARD: XOX......"),
        ("MOVE:4", "OK: BOARD: XOX.O...."),
        ("MOVE:3", "OK: BOARD: XOXOO...."),
        ("MOVE:5", "OK: BOARD: XOXOOX..."),
        ("MOVE:7", "OK: BOARD: XOXOOX.O."),
        ("MOVE:6", "OK: BOARD: XOXOOXO.."),
    ]
    for cmd, reply in expected:
        send_cmd(ser, cmd)
        assert read_line(ser) == reply

    send_cmd(ser, "MOVE:8")
    assert read_line(ser) == "DRAW"