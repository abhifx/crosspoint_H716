import serial
import time

try:
    ser = serial.Serial('COM5', 115200, timeout=0.1)
    print("Reading serial from COM5 (raw hex)...")
    start_time = time.time()
    while time.time() - start_time < 5:
        data = ser.read(64)
        if data:
            print(data.hex())
    ser.close()
except Exception as e:
    print(f"Error: {e}")
