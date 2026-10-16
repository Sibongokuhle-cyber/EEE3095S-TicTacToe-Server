import os
import time
import pytest
import serial

BOOT_TIMEOUT = 6.0
BOOT_MARKER = b"Waiting for CONNECT"


@pytest.fixture
def ser():
    port = os.environ.get("SERIAL_PORT")
    if not port:
        pytest.skip("Set SERIAL_PORT environment variable to run hardware tests")

    s = serial.Serial(port, 115200, timeout=0.5,
                      dsrdtr=False, rtscts=False)

    # Opening the port asserts DTR, which is wired to NRST on the ST-Link
    # Virtual COM Port -> the MCU resets. Wait for the boot banner so we
    # don't send commands before HAL_UART_Receive_IT is running.
    deadline = time.time() + BOOT_TIMEOUT
    saw_banner = False
    while time.time() < deadline:
        line = s.readline()
        if BOOT_MARKER in line:
            saw_banner = True
            break

    if not saw_banner:
        s.close()
        pytest.fail(
            f"MCU did not produce boot banner within {BOOT_TIMEOUT}s on {port}.\n"
            "Check: board powered, firmware flashed, correct COM port, "
            "115200 8N1."
        )

    # Discard anything else that may have arrived with the banner.
    time.sleep(0.1)
    s.reset_input_buffer()
    s.timeout = 2
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