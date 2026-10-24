"""pytest fixtures and helpers for the STM32 Tic-Tac-Toe server.

Run (hardware attached, GUI closed):
    pip install pytest pyserial
    set SERIAL_PORT=COM14          (Windows)   |   export SERIAL_PORT=/dev/ttyACM0
    pytest -v tests
    set USE_CRC=1  &&  pytest -v tests     # same suite, every request carries *HH

The fixture does NOT rely on the boot banner or on DTR resetting the board.
It syncs by sending CONNECT until the server answers OK, then puts the server
into a known state (IDLE, no session) before every test.
"""
import os
import time

import pytest
import serial

SYNC_TIMEOUT = 10.0
USE_CRC = os.environ.get("USE_CRC") == "1"


# ---------------------------------------------------------------- checksum
def crc8(data: bytes) -> int:
    """CRC-8, poly 0x07, init 0, no reflection, no final XOR (CRC-8/SMBUS).
    crc8(b"123456789") == 0xF4."""
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


def frame(cmd: str) -> bytes:
    """A command with a checksum suffix and terminator: b'MOVE:4*7B\\n'."""
    return f"{cmd}*{crc8(cmd.encode()):02X}\n".encode()


def split_checksum(line: str):
    """Return (payload, status): status None = no suffix, True = valid, False = wrong."""
    if len(line) >= 3 and line[-3] == "*":
        try:
            got = int(line[-2:], 16)
        except ValueError:
            return line, None
        payload = line[:-3]
        return payload, got == crc8(payload.encode())
    return line, None


# ----------------------------------------------------------------- helpers
def send_cmd(ser, cmd):
    data = frame(cmd) if USE_CRC else (cmd + "\n").encode()
    ser.write(data)
    ser.flush()


def send_raw(ser, data: bytes):
    ser.write(data)
    ser.flush()


def read_raw(ser):
    """One reply line exactly as received (checksum suffix included)."""
    raw = ser.readline().decode(errors="replace").strip()
    if not raw:
        pytest.fail("no response from device")
    return raw


def read_line(ser, expected=None):
    """One reply line with a checksum suffix verified and removed (if present)."""
    raw = ser.readline().decode(errors="replace").strip()
    if not raw:
        pytest.fail(f"no response from device (expected {expected!r})")
    payload, ok = split_checksum(raw)
    if ok is False:
        pytest.fail(f"reply has a wrong checksum: {raw!r}")
    return payload


def _sync(ser):
    """Send CONNECT until the server answers OK (skips boot banner lines)."""
    deadline = time.time() + SYNC_TIMEOUT
    while time.time() < deadline:
        send_cmd(ser, "CONNECT")
        t_end = time.time() + 0.8
        while time.time() < t_end:
            line = ser.readline().decode(errors="replace").strip()
            if split_checksum(line)[0] == "OK":
                return True
    return False


@pytest.fixture
def ser():
    port = os.environ.get("SERIAL_PORT")
    if not port:
        pytest.skip("Set SERIAL_PORT to run the hardware tests")

    s = serial.Serial(port, 115200, timeout=0.3, dsrdtr=False, rtscts=False)

    if not _sync(s):
        s.close()
        pytest.fail(f"No OK to CONNECT within {SYNC_TIMEOUT}s on {port}. "
                    "Check: board powered, firmware flashed, right COM port, 115200 8N1.")

    # Known state: drain stale replies (CONNECT retries), DISCONNECT -> IDLE/no session.
    time.sleep(0.3)
    s.reset_input_buffer()
    send_cmd(s, "DISCONNECT")
    s.timeout = 2
    assert split_checksum(s.readline().decode(errors="replace").strip())[0] == "OK", \
        "server did not acknowledge DISCONNECT"
    time.sleep(0.1)
    s.reset_input_buffer()

    yield s

    try:
        s.reset_input_buffer()
        send_cmd(s, "DISCONNECT")
        s.close()
    except Exception:
        pass
