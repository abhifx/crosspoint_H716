import serial
import time

ports = ["COM5", "COM7", "COM8"]
for port in ports:
    try:
        print(f"Checking {port}...")
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(0.5)
        data = ser.read(1000)
        if data:
            print(f"Data from {port}: {data.hex()[:100]}")
            print(f"Text from {port}: {data.decode('utf-8', errors='ignore')[:100]}")
        ser.close()
    except Exception as e:
        print(f"Error on {port}: {e}")
