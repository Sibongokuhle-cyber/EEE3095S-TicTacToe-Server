"""Quick link check: prints whatever the board sends, then pings it.

    python boot_test.py COM14        (or /dev/ttyACM0)
"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM14"
with serial.Serial(port, 115200, timeout=0.3) as s:
    print(f"Opened {port}. Press the RESET button now if you want to see the boot banner.")
    t_end = time.time() + 10
    last_ping = 0
    while time.time() < t_end:
        if time.time() - last_ping > 1.0:
            s.write(b"CONNECT\n")
            last_ping = time.time()
        line = s.readline().decode(errors="replace").strip()
        if line:
            print(f"[{10 - (t_end - time.time()):4.1f}s] {line}")
