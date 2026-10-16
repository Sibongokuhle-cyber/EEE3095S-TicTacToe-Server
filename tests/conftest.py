"""pytest fixtures for the STM32 Tic-Tac-Toe server.

Run (hardware attached):
    pip install pytest pyserial
    set SERIAL_PORT=COM14          (Windows)   |   export SERIAL_PORT=/dev/ttyACM0
    pytest -v tests

The fixture does NOT rely on the boot banner or on DTR resetting the board.
It syncs by sending CONNECT until the server answers OK, then puts the server
into a known state (IDLE, no session) before every test.
"""
import os
import time

import pytest
import serial

SYNC_TIMEOUT = 10.0


def send_cmd(ser, cmd):
    ser.write((cmd + "\n").encode())
    ser.flush()


def send_raw(ser, data: bytes):
    ser.write(data)
    ser.flush()


def read_line(ser, expected=None):
    raw = ser.readline().decode(errors="replace").strip()
    if not raw:
        pytest.fail(f"no response from device (expected {expected!r})")
    return raw


def _sync(ser):
    """Send CONNECT until the server answers OK (skips boot banner lines)."""
    deadline = time.time() + SYNC_TIMEOUT
    while time.time() < deadline:
        send_cmd(ser, "CONNECT")
        t_end = time.time() + 0.8
        while time.time() < t_end:
            if ser.readline().strip() == b"OK":
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
    assert s.readline().strip() == b"OK", "server did not acknowledge DISCONNECT"
    time.sleep(0.1)
    s.reset_input_buffer()

    yield s

    try:
        s.reset_input_buffer()
        send_cmd(s, "DISCONNECT")
        s.close()
    except Exception:
        pass