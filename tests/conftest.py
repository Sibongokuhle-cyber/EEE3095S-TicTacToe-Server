import os
import serial
import pytest

@pytest.fixture
def ser():
    port = os.environ.get("SERIAL_PORT")
    if not port:
        pytest.skip("Set SERIAL_PORT environment variable to run hardware tests")
    s = serial.Serial(port, 115200, timeout=1)
    yield s
    s.close()

def send_cmd(ser, cmd):
    ser.write((cmd + "\n").encode())

def read_line(ser):
    return ser.readline().decode(errors="ignore").strip()