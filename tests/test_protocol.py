from conftest import send_cmd, read_line

def test_connect(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser) == "OK"

def test_new(ser):
    send_cmd(ser, "CONNECT")
    assert read_line(ser) == "OK"
    send_cmd(ser, "NEW")
    assert read_line(ser) == "OK: BOARD: ........."

def test_legal_move(ser):
    send_cmd(ser, "NEW")
    assert read_line(ser) == "OK: BOARD: ........."
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "OK: BOARD: X........"

def test_occupied(ser):
    send_cmd(ser, "NEW")
    read_line(ser)
    send_cmd(ser, "MOVE:0")
    read_line(ser)
    send_cmd(ser, "MOVE:0")
    assert read_line(ser) == "NACK: OCCUPIED"
    send_cmd(ser, "STATE")
    assert read_line(ser) == "BOARD: X........"

def test_range(ser):
    send_cmd(ser, "NEW")
    read_line(ser)
    send_cmd(ser, "MOVE:9")
    assert read_line(ser) == "NACK: RANGE"

def test_after_game_over(ser):
    send_cmd(ser, "NEW")
    read_line(ser)
    for cmd in ["MOVE:0", "MOVE:3", "MOVE:1", "MOVE:4", "MOVE:2"]:
        send_cmd(ser, cmd)
        read_line(ser)
    send_cmd(ser, "MOVE:5")
    assert read_line(ser) in ("NACK: NOGAME", "NACK: BADCMD")

def test_win(ser):
    send_cmd(ser, "NEW")
    read_line(ser)
    seq = ["MOVE:0", "MOVE:3", "MOVE:1", "MOVE:4", "MOVE:2"]
    for cmd in seq[:-1]:
        send_cmd(ser, cmd)
        read_line(ser)
    send_cmd(ser, seq[-1])
    assert read_line(ser) == "WIN: X"

def test_draw(ser):
    send_cmd(ser, "NEW")
    read_line(ser)
    seq = ["MOVE:0", "MOVE:1", "MOVE:2", "MOVE:4",
           "MOVE:3", "MOVE:5", "MOVE:7", "MOVE:6", "MOVE:8"]
    for cmd in seq[:-1]:
        send_cmd(ser, cmd)
        read_line(ser)
    send_cmd(ser, seq[-1])
    assert read_line(ser) == "DRAW"