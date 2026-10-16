import serial
import time
import sys

PORT = "COM14"  # <-- CHANGE if different

try:
    s = serial.Serial(PORT, 115200, timeout=5)
    print(f"Opened {PORT}")
    print(">>> PRESS THE RESET BUTTON ON THE STM32 NOW <<<")
    print("Watching for 15 seconds...\n")

    start = time.time()
    while time.time() - start < 15:
        line = s.readline().decode(errors='ignore').strip()
        if line:
            print(f"[{time.time()-start:5.1f}s] {line}")
    s.close()
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)