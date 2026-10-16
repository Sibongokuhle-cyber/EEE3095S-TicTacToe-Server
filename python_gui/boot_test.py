import serial
import time

# CHANGE THIS to your actual COM port
PORT = "COM14"

try:
    s = serial.Serial(PORT, 115200, timeout=2)
    print(f"Opened {PORT}")
    time.sleep(1)  # Wait for boot message
    data = s.read_all()
    print("Boot message:")
    print(data.decode(errors='ignore'))
    s.close()
except Exception as e:
    print(f"ERROR: {e}")