import serial
import time

def read_logs():
    print("Attempting to connect to COM5...")
    while True:
        try:
            ser = serial.Serial('COM5', 115200, timeout=0.1)
            print("Connected! Reading logs (Ctrl+C to stop)...")
            while True:
                line = ser.readline()
                if line:
                    try:
                        print(line.decode('utf-8', errors='ignore').strip())
                    except:
                        pass
        except serial.SerialException:
            time.sleep(0.5)
        except KeyboardInterrupt:
            break

if __name__ == "__main__":
    read_logs()
