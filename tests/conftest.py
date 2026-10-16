import os
import pytest
import serial


@pytest.fixture
def ser():
    port = os.environ.get("SERIAL_PORT")
    if not port:
        pytest.skip("Set SERIAL_PORT environment variable to run hardware tests")
    s = serial.Serial(port, 115200, timeout=2)
    s.reset_input_buffer()
    yield s
    try:
        s.close()
    except Exception:
        pass


def send_cmd(ser, cmd):
    ser.write((cmd + "\n").encode())
    ser.flush()


def read_line(ser, expected=None):
    raw = ser.readline().decode(errors="ignore").strip()
    if not raw:
        pytest.fail(f"no response from device (expected {expected!r})")
    return raw